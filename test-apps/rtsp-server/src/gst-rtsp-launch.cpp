#include <iostream>
#include <string>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <cctype>
#include <iomanip>
#include <sstream>

#define DEFAULT_RTSP_PORT "8554"
#define DEFAULT_ENDPOINT "video"


#define DEFAULT_RTSP_USER "user"
#define DEFAULT_RTSP_PASSWORD "password"
#define DEFAULT_RTSP_AUTH "none"

std::string url_encode(const std::string& value);

int main(int argc, char** argv) {
    GMainLoop *loop;
    GstRTSPServer *server;
    GstRTSPMountPoints *mounts;
    GstRTSPMediaFactory *factory;
    GstRTSPAuth* auth;
    GstRTSPToken* token;
    GOptionContext *optctx;
    GError *error = NULL;

    struct {
        char *port = nullptr;
        char* endpoint = nullptr;
        char* auth = nullptr;
        char* username = nullptr;
        char* password = nullptr;
    } opts;

    GOptionEntry entries[] = {
        {"port", 'p', 0, G_OPTION_ARG_STRING, &opts.port, "Port to listen on (default: " DEFAULT_RTSP_PORT ")", "PORT"},
        {"endpoint", 'e', 0, G_OPTION_ARG_STRING, &opts.endpoint, "Endpoint name (default: " DEFAULT_ENDPOINT ")", "ENDPOINT"},
        {"auth", 'a', 0, G_OPTION_ARG_STRING, &opts.auth, "Auth (none, basic, digest) (default: " DEFAULT_RTSP_AUTH ")", "AUTH"},
        {"username", 'u', 0, G_OPTION_ARG_STRING, &opts.username, "Username (default: " DEFAULT_RTSP_USER ")", "USER"},
        {"password", 'P', 0, G_OPTION_ARG_STRING, &opts.password, "Password (default: " DEFAULT_RTSP_PASSWORD ")", "PASSWORD"},
        {nullptr}
    };

    optctx = g_option_context_new("<launch line> - Simple RTSP server\n\n"
        "Sample pipeline: \"( videotestsrc ! x264enc ! rtph264pay name=pay0 pt=96 )\"");
    g_option_context_add_main_entries(optctx, entries, NULL);
    g_option_context_add_group(optctx, gst_init_get_option_group());
    if (!g_option_context_parse(optctx, &argc, &argv, &error)) {
        g_printerr("Error parsing options: %s", error->message);
        g_option_context_free(optctx);
        g_clear_error(&error);
        return -1;
    }

    if (nullptr == argv[1]) {
        g_printerr("Error: empty pipeline\n");
        return -1;
    }

    g_option_context_free(optctx);

    if (nullptr == opts.port) {
        g_print("Using default port: %s\n", DEFAULT_RTSP_PORT);
        opts.port = strdup(DEFAULT_RTSP_PORT);
    } else {
        g_print("Using port: %s\n", opts.port);
    }

    if (nullptr == opts.endpoint) {
        g_print("Using default endpoint: %s\n", DEFAULT_ENDPOINT);
        opts.endpoint = strdup(DEFAULT_ENDPOINT);
    }
    else {
        g_print("Using endpoint: %s\n", opts.endpoint);
    }

    if (nullptr == opts.auth) {
        g_print("Using default auth: %s\n", DEFAULT_RTSP_AUTH);
        opts.auth = strdup(DEFAULT_RTSP_AUTH);
    }
    else {
        g_print("Using auth: %s\n", opts.auth);
    }

    if (nullptr == opts.username) {
        g_print("Using default username: %s\n", DEFAULT_RTSP_USER);
        opts.username = strdup(DEFAULT_RTSP_USER);
    }
    else {
        g_print("Using username: %s\n", opts.username);
    }

    if (nullptr == opts.password) {
        g_print("Using default password: %s\n", DEFAULT_RTSP_PASSWORD);
        opts.password = strdup(DEFAULT_RTSP_PASSWORD);
    }
    else {
        g_print("Using password: %s\n", opts.password);
    }


    loop = g_main_loop_new(NULL, FALSE);

    server = gst_rtsp_server_new();
    gst_rtsp_server_set_service(server, opts.port);
    mounts = gst_rtsp_server_get_mount_points(server);
    factory = gst_rtsp_media_factory_new();

    gst_rtsp_media_factory_set_launch(factory, argv[1]);
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    g_print("Using pipeline (as parsed): %s\n", gst_rtsp_media_factory_get_launch(factory));
    std::string mount = "/";
    mount += opts.endpoint;
    gst_rtsp_mount_points_add_factory(mounts, mount.c_str(), factory);

    if (std::string(opts.auth).compare("none") == 0) {
        g_print("Using auth none\n");
    }
    else if (std::string(opts.auth).compare("digest") == 0) {
        g_print("Using auth digest\n");
        /* allow user and admin to access this resource */
        gst_rtsp_media_factory_add_role(factory, "user",
            GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
            GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, TRUE, NULL);
        gst_rtsp_media_factory_add_role(factory, "anonymous",
            GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
            GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, FALSE, NULL);

        /* make a new authentication manager */
        auth = gst_rtsp_auth_new();
        gst_rtsp_auth_set_supported_methods(auth, GST_RTSP_AUTH_DIGEST);

        /* make default token, it has no permissions */
        token =
            gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING,
                "anonymous", NULL);
        gst_rtsp_auth_set_default_token(auth, token);
        gst_rtsp_token_unref(token);

        /* make user token */
        token =
            gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING,
                "user", NULL);

        gst_rtsp_auth_add_digest(auth, opts.username, opts.password, token);
        gst_rtsp_token_unref(token);

        /* set as the server authentication manager */
        gst_rtsp_server_set_auth(server, auth);
        g_object_unref(auth);
    }
    else if (std::string(opts.auth).compare("basic") == 0) {
        gchar* basic;
        g_print("Using auth basic\n");
        /* allow user and admin to access this resource */
        gst_rtsp_media_factory_add_role(factory, "user",
            GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
            GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, TRUE, NULL);
        gst_rtsp_media_factory_add_role(factory, "anonymous",
            GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
            GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, FALSE, NULL);

        /* make a new authentication manager */
        auth = gst_rtsp_auth_new();
        // gst_rtsp_auth_set_supported_methods(auth, GST_RTSP_AUTH_BASIC);

        /* make default token, it has no permissions */
        token =
            gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING,
                "anonymous", NULL);
        gst_rtsp_auth_set_default_token(auth, token);
        gst_rtsp_token_unref(token);

        /* make user token */
        token =
            gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING,
                "user", NULL);

        basic = gst_rtsp_auth_make_basic(opts.username, opts.password);
        gst_rtsp_auth_add_basic(auth, basic, token);
        g_free(basic);

        gst_rtsp_token_unref(token);

        /* set as the server authentication manager */
        gst_rtsp_server_set_auth(server, auth);
        g_object_unref(auth);
    }
    g_object_unref(mounts);

    gst_rtsp_server_attach(server, NULL);

    g_print("Stream ready at rtsp://%s:%s@127.0.0.1:%s/%s\n", url_encode(opts.username).c_str(), url_encode(opts.password).c_str(), opts.port, opts.endpoint);
    g_main_loop_run(loop);

    return 0;
}


std::string url_encode(const std::string& in) {
    std::stringstream out;

    for (auto i = in.begin(); i != in.end(); i++) {
        if (std::isalnum(*i) || *i == '-' || *i == '_' || *i == '.' || *i == '~') {
            out << *i;
        } else {
            out << std::hex << std::uppercase << '%' << std::setw(2) << int((unsigned char)*i) << std::nouppercase << std::dec;
        }
    }
    return out.str();
}
