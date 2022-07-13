#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "wlr-export-dmabuf-unstable-v1-client-protocol.h"

static void capture_frame();

static struct zwlr_export_dmabuf_manager_v1 *export_manager = NULL;
static struct wl_output *output = NULL;
static EGLDisplay egl_display;
static uint32_t frame_width;
static uint32_t frame_height;
static uint32_t frame_format;
static uint32_t frame_planes;
static int32_t frame_fds[4];
static uint32_t frame_offsets[4];
static uint32_t frame_strides[4];
static uint64_t frame_modifiers[4];

static void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, zwlr_export_dmabuf_manager_v1_interface.name) == 0) {
		export_manager = wl_registry_bind(registry, name, &zwlr_export_dmabuf_manager_v1_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		if (!output)
			output = wl_registry_bind(registry, name, &wl_output_interface, 4);
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = NULL,
};

static void frame(void *data, struct zwlr_export_dmabuf_frame_v1 *, uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t buffer_flags, uint32_t flags, uint32_t format, uint32_t mod_high, uint32_t mod_low, uint32_t obj_count) {
	frame_width = width;
	frame_height = height;
	frame_format = format;
	frame_planes = obj_count;
	for (int i = 0; i < 4; ++i)
		frame_modifiers[i] = (((uint64_t) mod_high) << 32) | mod_low;
}

static void ready(void* data, struct zwlr_export_dmabuf_frame_v1* frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
	EGLAttrib attribs[47];
	int atti = 0;
	EGLImageKHR image;
	GLuint texture;
	GLuint fbo;
	uint32_t pixels[1];

	attribs[atti++] = EGL_WIDTH;
	attribs[atti++] = frame_width;
	attribs[atti++] = EGL_HEIGHT;
	attribs[atti++] = frame_height;
	attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
	attribs[atti++] = frame_format;

	if (frame_planes > 0) {
		attribs[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
		attribs[atti++] = frame_fds[0];
		attribs[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
		attribs[atti++] = frame_offsets[0];
		attribs[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
		attribs[atti++] = frame_strides[0];
		attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
		attribs[atti++] = frame_modifiers[0] & 0xFFFFFFFF;
		attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
		attribs[atti++] = frame_modifiers[0] >> 32;
	}

	if (frame_planes > 1) {
		attribs[atti++] = EGL_DMA_BUF_PLANE1_FD_EXT;
		attribs[atti++] = frame_fds[1];
		attribs[atti++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
		attribs[atti++] = frame_offsets[1];
		attribs[atti++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
		attribs[atti++] = frame_strides[1];
		attribs[atti++] = EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
		attribs[atti++] = frame_modifiers[1] & 0xFFFFFFFF;
		attribs[atti++] = EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
		attribs[atti++] = frame_modifiers[1] >> 32;
	}

	if (frame_planes > 2) {
		attribs[atti++] = EGL_DMA_BUF_PLANE2_FD_EXT;
		attribs[atti++] = frame_fds[2];
		attribs[atti++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
		attribs[atti++] = frame_offsets[2];
		attribs[atti++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
		attribs[atti++] = frame_strides[2];
		attribs[atti++] = EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
		attribs[atti++] = frame_modifiers[2] & 0xFFFFFFFF;
		attribs[atti++] = EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
		attribs[atti++] = frame_modifiers[2] >> 32;
	}

	if (frame_planes > 3) {
		attribs[atti++] = EGL_DMA_BUF_PLANE3_FD_EXT;
		attribs[atti++] = frame_fds[3];
		attribs[atti++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
		attribs[atti++] = frame_offsets[3];
		attribs[atti++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
		attribs[atti++] = frame_strides[3];
		attribs[atti++] = EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT;
		attribs[atti++] = frame_modifiers[3] & 0xFFFFFFFF;
		attribs[atti++] = EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT;
		attribs[atti++] = frame_modifiers[3] >> 32;
	}

	attribs[atti++] = EGL_NONE;

	image = eglCreateImage(egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
	if (image == EGL_NO_IMAGE_KHR) {
		fprintf(stderr, "Failed to create EGL image.\n");
	} else {
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
		glEGLImageTargetTexture2DOES(GL_TEXTURE_2D,image);

		glGenFramebuffers(1, &fbo); 
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
		glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		printf("GL error: %d\n", glGetError());
		printf("%u\n", pixels[0]);

		glDeleteTextures(1, &texture);
		eglDestroyImage(egl_display,image); 
	}

	for (int i = 0; i < frame_planes; ++i)
		close(frame_fds[i]);

	zwlr_export_dmabuf_frame_v1_destroy(frame);
	capture_frame();
}

static void cancel(void* data, struct zwlr_export_dmabuf_frame_v1* frame, enum zwlr_export_dmabuf_frame_v1_cancel_reason reason)
{
	printf("cancel\n");
}

static void object(void* data, struct zwlr_export_dmabuf_frame_v1* frame, uint32_t index, int32_t fd, uint32_t size, uint32_t offset, uint32_t stride, uint32_t plane_index)
{
	if (index > 3) {
		close(fd);
		return;
	}
	frame_fds[index] = fd;
	frame_strides[index] = stride;
	frame_offsets[index] = offset;
}

static const struct zwlr_export_dmabuf_frame_v1_listener frame_listener = {
	.frame = frame,
    .ready = ready,
	.object = object,
	.cancel = cancel,
};

static void capture_frame()
{
	struct zwlr_export_dmabuf_frame_v1 *frame = zwlr_export_dmabuf_manager_v1_capture_output(export_manager, 0, output);
	zwlr_export_dmabuf_frame_v1_add_listener(frame, &frame_listener, NULL);
}

int main()
{
	struct wl_display *display;
	struct wl_registry *registry;
	EGLint major, minor;
	EGLint attribs[3];
	int atti;
	EGLint num_config;
	EGLConfig egl_config;
	EGLContext egl_context;

	display = wl_display_connect(NULL);
	if (!display) {
		fprintf(stderr, "Cannot connect to Wayland display.\n");
		return 1;
	}
   
    egl_display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, display, NULL);
	if (eglInitialize(egl_display, &major, &minor) != EGL_TRUE) {
		fprintf(stderr, "Failed to initizlize EGL.\n");
		return 1;
	}

	atti = 0;
	attribs[atti++] = EGL_NONE;
	if (eglChooseConfig(egl_display, attribs, &egl_config, 1, &num_config) != EGL_TRUE) {
		fprintf(stderr, "Failed to choose EGL configuration.\n");
		return 1;
	}

	atti = 0;
	attribs[atti++] = EGL_CONTEXT_CLIENT_VERSION;
	attribs[atti++] = 3;
	attribs[atti++] = EGL_NONE;
	egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, attribs);
	if (egl_context == EGL_NO_CONTEXT) {
		fprintf(stderr, "Failed to create EGL context.\n");
		return 1;
	}

	if (eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context) != EGL_TRUE) {
		fprintf(stderr, "Failed to make EGL context current.\n");
		return 1;
	}

	printf("OpenGL version: %s\n", glGetString(GL_VERSION));

	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);

	wl_display_dispatch(display);

	if (!export_manager) {
		fprintf(stderr, "Missing wlr-export-dmabuf-manager-v1 protocol.\n");
		return 1;
	}

	if (!output) {
		fprintf(stderr, "No output.\n");
		return 1;
	}

    capture_frame();

	for (;;)
		wl_display_dispatch(display);
}
