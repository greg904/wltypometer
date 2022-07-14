#define _GNU_SOURCE
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>

#include "virtual-keyboard-unstable-v1-client-protocol.h"
#include "wlr-export-dmabuf-unstable-v1-client-protocol.h"

static struct wl_display *wl_display = NULL;
static struct zwlr_export_dmabuf_manager_v1 *export_manager = NULL;
static struct wl_output *output = NULL;
struct wl_seat *seat = NULL;
struct zwp_virtual_keyboard_manager_v1 *virt_kbd_manager = NULL;
struct zwp_virtual_keyboard_v1 *virt_kbd = NULL;
static EGLDisplay egl_display = EGL_NO_DISPLAY;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
static GLuint gl_fbo;
static GLuint gl_texture;
static uint32_t frame_width;
static uint32_t frame_height;
static uint32_t frame_format;
static uint32_t frame_planes;
static int32_t frame_fds[4];
static uint32_t frame_offsets[4];
static uint32_t frame_strides[4];
static uint64_t frame_modifiers[4];
static uint32_t watch_x;
static uint32_t watch_y;
static uint32_t watch_width;
static uint32_t watch_height;
static uint32_t *original = NULL;
static uint32_t *pixels = NULL;
static struct timespec input_time;
static int has_original = 0;
static int pressed_key = 0;
static int error = 0;

static void capture_frame();

static void registry_global(void *data, struct wl_registry *registry,
			    uint32_t name, const char *interface,
			    uint32_t version) {
	if (!strcmp(interface, zwlr_export_dmabuf_manager_v1_interface.name)) {
		export_manager = wl_registry_bind(
			registry, name,
			&zwlr_export_dmabuf_manager_v1_interface, 1);
	} else if (!strcmp(interface,
			   zwp_virtual_keyboard_manager_v1_interface.name)) {
		virt_kbd_manager = wl_registry_bind(
			registry, name,
			&zwp_virtual_keyboard_manager_v1_interface, 1);
	} else if (!strcmp(interface, wl_output_interface.name)) {
		if (!output)
			output = wl_registry_bind(registry, name,
						  &wl_output_interface, 4);
	} else if (!strcmp(interface, wl_seat_interface.name)) {
		if (!seat)
			seat = wl_registry_bind(registry, name,
						&wl_seat_interface, 7);
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = NULL,
};

static void frame(void *data, struct zwlr_export_dmabuf_frame_v1 *,
		  uint32_t width, uint32_t height, uint32_t x, uint32_t y,
		  uint32_t buffer_flags, uint32_t flags, uint32_t format,
		  uint32_t mod_high, uint32_t mod_low, uint32_t obj_count) {
	if (obj_count >= 4) {
		fputs("Too many planes.\n", stderr);
		error = 1;
	}

	if (error)
		return;

	frame_width = width;
	frame_height = height;
	frame_format = format;
	frame_planes = obj_count;

	for (int i = 0; i < 4; ++i) {
		frame_fds[i] = -1;
		frame_modifiers[i] = (((uint64_t)mod_high) << 32) | mod_low;
	}
}

static int reacted() {
	for (int y = 0; y < watch_height; ++y) {
		for (int x = 0; x < watch_width; ++x) {
			if (pixels[y * watch_width + x] !=
			    original[y * watch_width + x])
				return 1;
		}
	}

	return 0;
}

static EGLImageKHR egl_image_from_frame() {
	EGLAttrib attribs[47];
	int atti = 0;

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

	return eglCreateImage(egl_display, EGL_NO_CONTEXT,
			      EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
}

static int read_egl_image_pixels(EGLImageKHR image, uint32_t x, uint32_t y,
				 uint32_t width, uint32_t height,
				 uint32_t *pixels) {
	int err;

	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

	err = glGetError();
	if (err != GL_NO_ERROR) {
		fprintf(stderr, "glEGLImageTargetTexture2DOES failed: %d.",
			err);
		return -1;
	}

	glReadPixels(watch_x, watch_y, watch_width, watch_height, GL_RGBA,
		     GL_UNSIGNED_BYTE, pixels);

	err = glGetError();
	if (err != GL_NO_ERROR) {
		fprintf(stderr, "glReadPixels failed: %d.", err);
		return -1;
	}

	return 0;
}

static int check_reacted() {
	struct timespec now;
	long diff;

	if (!reacted())
		return 0;

	clock_gettime(CLOCK_MONOTONIC, &now);
	diff = now.tv_nsec - input_time.tv_nsec +
	       (now.tv_sec - input_time.tv_sec) * 1000000000;
	printf("%ld\n", diff);

	/* Send backspace. */
	zwp_virtual_keyboard_v1_key(virt_kbd, 0, 2,
				    WL_KEYBOARD_KEY_STATE_PRESSED);
	zwp_virtual_keyboard_v1_key(virt_kbd, 0, 2,
				    WL_KEYBOARD_KEY_STATE_RELEASED);

	if (wl_display_roundtrip(wl_display) == -1) {
		fputs("Failed to send backspace.\n", stderr);
		return -1;
	}

	pressed_key = 0;

	return 0;
}

static int check_ready_to_press() {
	if (reacted())
		return 0;

	/* Send an 'a'. */
	zwp_virtual_keyboard_v1_key(virt_kbd, 0, 1,
				    WL_KEYBOARD_KEY_STATE_PRESSED);
	zwp_virtual_keyboard_v1_key(virt_kbd, 0, 1,
				    WL_KEYBOARD_KEY_STATE_RELEASED);

	if (wl_display_roundtrip(wl_display) == -1) {
		fputs("Failed to send key press.\n", stderr);
		return -1;
	}

	clock_gettime(CLOCK_MONOTONIC, &input_time);
	pressed_key = 1;

	return 0;
}

static void ready(void *data, struct zwlr_export_dmabuf_frame_v1 *frame,
		  uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
	EGLImageKHR egl_image = EGL_NO_IMAGE_KHR;
	int ret;

	if (error)
		goto cleanup;

	egl_image = egl_image_from_frame();
	if (egl_image == EGL_NO_IMAGE_KHR) {
		fputs("Failed to create EGL image.\n", stderr);
		error = 1;
		goto cleanup;
	}

	if (read_egl_image_pixels(egl_image, watch_x, watch_y, watch_width,
				  watch_height, pixels) == -1) {
		fputs("Failed read pixels from EGL image.\n", stderr);
		error = 1;
		goto cleanup;
	}

	if (has_original) {
		if (pressed_key) {
			ret = check_reacted();
		} else {
			ret = check_ready_to_press();
		}

		if (ret == -1) {
			error = 1;
			goto cleanup;
		}
	} else {
		memcpy(original, pixels, watch_width * watch_height * 4);
		has_original = 1;
	}

	capture_frame();

cleanup:
	if (egl_image != EGL_NO_IMAGE_KHR)
		eglDestroyImage(egl_display, egl_image);

	for (int i = 0; i < frame_planes; ++i) {
		if (frame_fds[i] != -1)
			close(frame_fds[i]);
	}

	zwlr_export_dmabuf_frame_v1_destroy(frame);
}

static void cancel(void *data, struct zwlr_export_dmabuf_frame_v1 *frame,
		   enum zwlr_export_dmabuf_frame_v1_cancel_reason reason) {
	fputs("Received cancel from frame.\n", stderr);
	error = 1;
}

static void object(void *data, struct zwlr_export_dmabuf_frame_v1 *frame,
		   uint32_t index, int32_t fd, uint32_t size, uint32_t offset,
		   uint32_t stride, uint32_t plane_index) {
	if (index >= frame_planes) {
		fputs("Unexpected object index.\n", stderr);
		error = 1;
	}

	if (error) {
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

static void capture_frame() {
	struct zwlr_export_dmabuf_frame_v1 *frame =
		zwlr_export_dmabuf_manager_v1_capture_output(export_manager, 0,
							     output);
	if (zwlr_export_dmabuf_frame_v1_add_listener(frame, &frame_listener,
						     NULL) == -1) {
		fputs("Failed to add listener to frame.\n", stderr);
		error = 1;
	}
}

int parse_pair(const char *str, char sep, uint32_t *a, uint32_t *b) {
	char *end;
	long tmp;

	tmp = strtol(str, &end, 10);
	if (tmp > UINT32_MAX)
		return 0;

	*a = (uint32_t)tmp;

	if (*end != sep)
		return 0;

	tmp = strtol(end + 1, &end, 10);
	if (tmp > UINT32_MAX)
		return 0;

	*b = (uint32_t)tmp;

	return *end == '\0';
}

int parse_args(int argc, char **argv) {
	if (argc != 3)
		return -1;

	if (!parse_pair(argv[1], ',', &watch_x, &watch_y) ||
	    !parse_pair(argv[2], 'x', &watch_width, &watch_height))
		return -1;

	return 0;
}

static const char *keymap =
	"xkb_keymap {\n"
	"    xkb_keycodes \"(unnamed)\" {\n"
	"        minimum = 8;\n"
	"        maximum = 11;\n"
	"        \n"
	"        <K1> = 9;\n"
	"        <K2> = 10;\n"
	"    };\n"
	"\n"
	"    xkb_types \"(unnamed)\" { include \"complete\" };\n"
	"    xkb_compatibility \"(unnamed)\" { include \"complete\" };\n"
	"\n"
	"    xkb_symbols \"(unnamed)\" {\n"
	"        key <K1> {[a]};\n"
	"        key <K2> {[BackSpace]};\n"
	"    };\n"
	"};\n";

static int upload_keymap() {
	int fd;
	int ret = 0;

	fd = memfd_create("keymap", MFD_CLOEXEC);
	if (fd == -1)
		goto end;

	/* Note: the NUL-byte at the end is required. */
	if (write(fd, keymap, strlen(keymap) + 1) == -1) {
		ret = -1;
		goto end;
	}

	zwp_virtual_keyboard_v1_keymap(virt_kbd,
				       WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd,
				       strlen(keymap) + 1);

	if (wl_display_roundtrip(wl_display) == -1) {
		ret = -1;
		goto end;
	}

end:
	if (fd != -1)
		close(fd);
	return ret;
}

int main(int argc, char **argv) {
	int ret = EXIT_SUCCESS;
	EGLint major, minor;
	EGLint num_config;
	EGLConfig egl_config;
	EGLint attribs[3];
	int atti;
	struct wl_registry *registry;
	EGLContext egl_context = EGL_NO_CONTEXT;

	if (parse_args(argc, argv) == -1) {
		fprintf(stderr, "Usage: %s X,Y WIDTHxHEIGHT\n", argv[0]);
		ret = EXIT_FAILURE;
		goto end;
	}

	pixels = malloc(watch_width * watch_height * 4);
	original = malloc(watch_width * watch_height * 4);
	if (!pixels || !original) {
		fputs("Failed to allocate memory.\n", stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	wl_display = wl_display_connect(NULL);
	if (!wl_display) {
		fputs("Cannot connect to Wayland display.\n", stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	egl_display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR,
					    wl_display, NULL);
	if (eglInitialize(egl_display, &major, &minor) != EGL_TRUE) {
		fputs("Failed to initizlize EGL.\n", stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	atti = 0;
	attribs[atti++] = EGL_NONE;
	if (eglChooseConfig(egl_display, attribs, &egl_config, 1,
			    &num_config) != EGL_TRUE) {
		fputs("Failed to choose EGL configuration.\n", stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	atti = 0;
	attribs[atti++] = EGL_CONTEXT_CLIENT_VERSION;
	attribs[atti++] = 3;
	attribs[atti++] = EGL_NONE;
	egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT,
				       attribs);
	if (egl_context == EGL_NO_CONTEXT) {
		fputs("Failed to create EGL context.\n", stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	if (eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
			   egl_context) != EGL_TRUE) {
		fputs("Failed to make EGL context current.\n", stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	glEGLImageTargetTexture2DOES =
		(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress(
			"glEGLImageTargetTexture2DOES");
	if (!glEGLImageTargetTexture2DOES) {
		fputs("Missing glEGLImageTargetTexture2DOES.\n", stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	glGenFramebuffers(1, &gl_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, gl_fbo);

	glGenTextures(1, &gl_texture);
	glBindTexture(GL_TEXTURE_2D, gl_texture);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, gl_texture, 0);

	registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(registry, &registry_listener, NULL);

	wl_display_dispatch(wl_display);

	if (!export_manager) {
		fputs("Missing wlr_export_dmabuf_manager_v1 protocol.\n",
		      stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	if (!virt_kbd_manager) {
		fputs("Missing virtual_keyboard_unstable_v1 protocol.\n",
		      stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	if (!output) {
		fputs("No output.\n", stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	if (!seat) {
		fputs("No seat.\n", stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	virt_kbd = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
		virt_kbd_manager, seat);

	if (upload_keymap() == -1) {
		fputs("Failed to upload keymap.\n", stderr);
		ret = EXIT_FAILURE;
		goto end;
	}

	capture_frame();

	while (!error)
		wl_display_dispatch(wl_display);

end:
	if (egl_context != EGL_NO_CONTEXT)
		eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
			       EGL_NO_CONTEXT);
	if (egl_display != EGL_NO_DISPLAY)
		eglTerminate(egl_display);
	if (wl_display)
		wl_display_disconnect(wl_display);

	if (pixels)
		free(pixels);
	if (original)
		free(original);

	return ret;
}
