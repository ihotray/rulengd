#include <json-c/json.h>
#include <libubox/blobmsg_json.h>
#include <libubox/blobmsg.h>
#include <libubus.h>


int counter;
/*
static void send_restricted_event(struct uloop_timeout *t);
static void send_event(struct uloop_timeout *t);


struct uloop_timeout restricted_event = {
	.cb = send_restricted_event
};

struct uloop_timeout event = {
	.cb = send_event
};

static void send_event(struct uloop_timeout *t)
{
    struct blob_buf b = {0};

    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "test", "success");
    ubus_send_event(ctx, "template", b.head);
    blob_buf_free(&b);

	uloop_timeout_set(&event, 2 * 1000);
}

static void send_restricted_event(struct uloop_timeout *t)
{
    struct blob_buf b = {0};

    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "test", "success");
    ubus_send_event(ctx, "restricted_template", b.head);
    blob_buf_free(&b);

	uloop_timeout_set(&restricted_event, 2 * 1000);
}
*/

int increment(struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request_data *req, const char *method,
		  struct blob_attr *msg)
{
	counter++;

	return 0;
}

int reset(struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request_data *req, const char *method,
		  struct blob_attr *msg)
{
	counter = 0;

	return 0;
}

int status(struct ubus_context *ctx, struct ubus_object *obj,
		  struct ubus_request_data *req, const char *method,
		  struct blob_attr *msg)
{
    struct blob_buf b = {0};

    blob_buf_init(&b, 0);
    blobmsg_add_u32(&b, "counter", counter);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);

	return 0;
}

struct ubus_method template_object_methods[] = {
	UBUS_METHOD_NOARG("increment", increment),
	UBUS_METHOD_NOARG("reset", reset),
	UBUS_METHOD_NOARG("status", status),
};

struct ubus_object_type template_object_type =
	UBUS_OBJECT_TYPE("template", template_object_methods);

struct ubus_object template_object = {
	.name = "template",
	.type = &template_object_type,
	.methods = template_object_methods,
	.n_methods = ARRAY_SIZE(template_object_methods),
};

struct ubus_object template_copy_object = {
	.name = "template_copy",
	.type = &template_object_type,
	.methods = template_object_methods,
	.n_methods = ARRAY_SIZE(template_object_methods),
};

int main(int argc, char **argv)
{
    int ret;
	struct ubus_context *ctx;

	uloop_init();

	ctx = ubus_connect(NULL);
	if (!ctx)
			return -EIO;

	printf("connected as %08x\n", ctx->local_id);
	ubus_add_uloop(ctx);
	ret = ubus_add_object(ctx, &template_object);
    if (ret != 0)
            fprintf(stderr, "Failed to publish object '%s': %s\n", template_object.name, ubus_strerror(ret));

	ret = ubus_add_object(ctx, &template_copy_object);
    if (ret != 0)
            fprintf(stderr, "Failed to publish object '%s': %s\n", template_copy_object.name, ubus_strerror(ret));

	/*
	uloop_timeout_set(&event, 2 * 1000);
	uloop_timeout_set(&restricted_event, 2 * 1000);
	*/

	uloop_run();
	ubus_free(ctx);

	return 0;
}
