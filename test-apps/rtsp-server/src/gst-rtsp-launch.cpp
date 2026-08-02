#include <iostream>
#include <string>
#include <vector>
#include <cstring>
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
#define DEFAULT_PATH_PATTERN "/live/%%CHANNEL%%/1"
#define CHANNEL_PLACEHOLDER "%%CHANNEL%%"

// Helper function to replace %%CHANNEL%% with channel number
std::string make_mount_path(const char* pattern, int channel) {
    std::string path(pattern);
    size_t pos = path.find(CHANNEL_PLACEHOLDER);
    if (pos != std::string::npos) {
        path.replace(pos, strlen(CHANNEL_PLACEHOLDER), std::to_string(channel));
    }
    return path;
}

// Helper to inject textoverlay into pipeline for channel identification
std::string make_pipeline(const char* base_pipeline, int channel, int total_streams) {
    std::string pipeline(base_pipeline);
    // Only add textoverlay if we have multiple streams
    if (total_streams > 1) {
        // Find videotestsrc or first element and inject textoverlay after first !
        size_t pos = pipeline.find("!");
        if (pos != std::string::npos) {
            std::string overlay = " textoverlay text=\"Stream " + std::to_string(channel) +
                "\" valignment=top halignment=left font-desc=\"Sans, 24\" !";
            pipeline.insert(pos + 1, overlay);
        }
    }
    return pipeline;
}

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
        int streams = 1;
        char* path_pattern = nullptr;
    } opts;

    GOptionEntry entries[] = {
        {"port", 'p', 0, G_OPTION_ARG_STRING, &opts.port, "Port to listen on (default: " DEFAULT_RTSP_PORT ")", "PORT"},
        {"endpoint", 'e', 0, G_OPTION_ARG_STRING, &opts.endpoint, "Endpoint name (default: " DEFAULT_ENDPOINT ")", "ENDPOINT"},
        {"auth", 'a', 0, G_OPTION_ARG_STRING, &opts.auth, "Auth (none, basic, digest) (default: " DEFAULT_RTSP_AUTH ")", "AUTH"},
        {"username", 'u', 0, G_OPTION_ARG_STRING, &opts.username, "Username (default: " DEFAULT_RTSP_USER ")", "USER"},
        {"password", 'P', 0, G_OPTION_ARG_STRING, &opts.password, "Password (default: " DEFAULT_RTSP_PASSWORD ")", "PASSWORD"},
        {"streams", 's', 0, G_OPTION_ARG_INT, &opts.streams, "Number of streams to create (default: 1)", "N"},
        {"path-pattern", 0, 0, G_OPTION_ARG_STRING, &opts.path_pattern, "Path pattern with %%CHANNEL%% placeholder (default: /live/%%CHANNEL%%/1)", "PATTERN"},
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

    // Determine if using multi-stream mode (--streams) or legacy single stream (--endpoint)
    bool use_multi_stream = (opts.streams > 1) || (opts.path_pattern != nullptr);
    const char* path_pattern = opts.path_pattern ? opts.path_pattern : DEFAULT_PATH_PATTERN;

    g_print("Using base pipeline: %s\n", argv[1]);
    g_print("Creating %d stream(s)\n", opts.streams);

    // Store factories for auth configuration
    std::vector<GstRTSPMediaFactory*> factories;

    for (int i = 1; i <= opts.streams; i++) {
        factory = gst_rtsp_media_factory_new();

        // Create pipeline with optional stream identifier overlay
        std::string pipeline = make_pipeline(argv[1], i, opts.streams);
        gst_rtsp_media_factory_set_launch(factory, pipeline.c_str());
        gst_rtsp_media_factory_set_shared(factory, TRUE);

        // Determine mount path
        std::string mount;
        if (use_multi_stream) {
            mount = make_mount_path(path_pattern, i);
        } else {
            mount = "/";
            mount += opts.endpoint;
        }

        gst_rtsp_mount_points_add_factory(mounts, mount.c_str(), factory);
        factories.push_back(factory);

        g_print("Stream %d ready at rtsp://127.0.0.1:%s%s\n", i, opts.port, mount.c_str());
    }

    // Configure authentication
    if (std::string(opts.auth).compare("none") == 0) {
        g_print("Using auth none\n");
    }
    else if (std::string(opts.auth).compare("digest") == 0) {
        g_print("Using auth digest\n");

        // Add roles to all factories
        for (auto* f : factories) {
            gst_rtsp_media_factory_add_role(f, "user",
                GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
                GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, TRUE, NULL);
            gst_rtsp_media_factory_add_role(f, "anonymous",
                GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
                GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, FALSE, NULL);
        }

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

        // Add roles to all factories
        for (auto* f : factories) {
            gst_rtsp_media_factory_add_role(f, "user",
                GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
                GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, TRUE, NULL);
            gst_rtsp_media_factory_add_role(f, "anonymous",
                GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
                GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, FALSE, NULL);
        }

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

    g_print("Server running on port %s\n", opts.port);
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
