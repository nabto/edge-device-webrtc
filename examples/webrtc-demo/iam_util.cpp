#include "nabto_device.hpp"
#include <nabto/nabto_device_webrtc.hpp>

#include <modules/iam/nm_iam_serializer.h>

#include <random>

namespace example {

std::string passwordGen(size_t len)
{
    // Same alphabet used by tcp_tunnel_device
    const std::string alphabet = "abcdefghijkmnopqrstuvwxyzACEFHJKLMNPRTUVWXY3479";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, alphabet.size() - 1);

    std::string password;

    for (std::size_t i = 0; i < len; ++i)
    {
        password += alphabet[distribution(generator)];
    }

    return password;
}

bool NabtoDeviceApp::createDefaultIamState()
{
    struct nm_iam_state* state = nm_iam_state_new();
    std::string initialUser = "admin";
    std::string openPassword = passwordGen(12);
    std::string openSct;

    {
        char* sct = NULL;
        if (nabto_device_create_server_connect_token(device_.get(), &sct) != NABTO_DEVICE_EC_OK) {
            nm_iam_state_free(state);
            return false;
        }
        openSct = std::string(sct);
        nabto_device_string_free(sct);
    }

    if (!nm_iam_state_set_password_open_password(state, openPassword.c_str()) ||
        !nm_iam_state_set_password_open_sct(state, openSct.c_str()) ||
        !nm_iam_state_set_open_pairing_role(state, "Administrator") ||
        !nm_iam_state_set_friendly_name(state, "Webrtc demo example") ||
        !nm_iam_state_set_initial_pairing_username(state, initialUser.c_str())
        )
    {
        nm_iam_state_free(state);
        return false;
    }

    nm_iam_state_set_password_open_pairing(state, true);
    nm_iam_state_set_local_open_pairing(state, true);
    nm_iam_state_set_local_initial_pairing(state, true);
    nm_iam_state_set_password_invite_pairing(state, true);

    {
        struct nm_iam_user* user = nm_iam_state_user_new(initialUser.c_str());
        if (user == NULL) {
            nm_iam_state_free(state);
            return false;
        }

        {
            char* sct = NULL;
            if (nabto_device_create_server_connect_token(device_.get(), &sct) != NABTO_DEVICE_EC_OK ||
                !nm_iam_state_user_set_sct(user, sct)) {
                nabto_device_string_free(sct);
                nm_iam_state_user_free(user);
                nm_iam_state_free(state);
                return false;
            }
            nabto_device_string_free(sct);
        }
        std::string userPwd = passwordGen(12);

        if (!nm_iam_state_user_set_role(user, "Administrator") ||
            !nm_iam_state_user_set_password(user, userPwd.c_str())) {
            nm_iam_state_user_free(user);
            nm_iam_state_free(state);
            return false;
        }
        nm_iam_state_add_user(state, user);
    }

    try {
        char* jsonState = NULL;
        if (!nm_iam_serializer_state_dump_json(state, &jsonState)) {
            nm_iam_state_free(state);
            return false;
        }
        std::string stateStr(jsonState);
        nm_iam_serializer_string_free(jsonState);

        std::ofstream stateFile(iamStatePath_);
        stateFile << stateStr;
    }
    catch (std::exception& ex) {
        NPLOGE << "Failed to write to state file: " << iamStatePath_ << " exception: " << ex.what();
        nm_iam_state_free(state);
        return false;
    }
    nm_iam_state_free(state);
    return true;
}

bool NabtoDeviceApp::createDefaultIamConfig()
{
    struct nm_iam_configuration* conf = nm_iam_configuration_new();
    {
        // Policy allowing pairing
        struct nm_iam_policy* p = nm_iam_configuration_policy_new("Pairing");
        struct nm_iam_statement* s = nm_iam_configuration_policy_create_statement(p, NM_IAM_EFFECT_ALLOW);
        nm_iam_configuration_statement_add_action(s, "IAM:GetPairing");
        nm_iam_configuration_statement_add_action(s, "IAM:PairingPasswordOpen");
        nm_iam_configuration_statement_add_action(s, "IAM:PairingPasswordInvite");
        nm_iam_configuration_statement_add_action(s, "IAM:PairingLocalInitial");
        nm_iam_configuration_statement_add_action(s, "IAM:PairingLocalOpen");
        nm_iam_configuration_add_policy(conf, p);
    }
    {
        // Policy allowing webrtc streaming
        struct nm_iam_policy* p = nm_iam_configuration_policy_new("Webrtc");
        struct nm_iam_statement* s = nm_iam_configuration_policy_create_statement(p, NM_IAM_EFFECT_ALLOW);
        nm_iam_configuration_statement_add_action(s, "Webrtc:VideoStream");
        nm_iam_configuration_statement_add_action(s, "Webrtc:FileStream");
        nm_iam_configuration_add_policy(conf, p);
    }
    {
        // Policy allowing webrtc signaling
        struct nm_iam_policy* p = nm_iam_configuration_policy_new("WebrtcSignaling");
        struct nm_iam_statement* s = nm_iam_configuration_policy_create_statement(p, NM_IAM_EFFECT_ALLOW);
        nm_iam_configuration_statement_add_action(s, "Webrtc:Signaling");
        nm_iam_configuration_statement_add_action(s, "Webrtc:GetInfo");
        nm_iam_configuration_add_policy(conf, p);
    }
    {
        // Policy allowing management of IAM state
        struct nm_iam_policy* p = nm_iam_configuration_policy_new("ManageIam");
        struct nm_iam_statement* s = nm_iam_configuration_policy_create_statement(p, NM_IAM_EFFECT_ALLOW);
        nm_iam_configuration_statement_add_action(s, "IAM:ListUsers");
        nm_iam_configuration_statement_add_action(s, "IAM:GetUser");
        nm_iam_configuration_statement_add_action(s, "IAM:DeleteUser");
        nm_iam_configuration_statement_add_action(s, "IAM:SetUserRole");
        nm_iam_configuration_statement_add_action(s, "IAM:ListRoles");
        nm_iam_configuration_statement_add_action(s, "IAM:CreateUser");
        nm_iam_configuration_statement_add_action(s, "IAM:SetUserPassword");
        nm_iam_configuration_statement_add_action(s, "IAM:SetUserDisplayName");
        nm_iam_configuration_statement_add_action(s, "IAM:SetUserOauthSubject");
        nm_iam_configuration_statement_add_action(s, "IAM:SetSettings");
        nm_iam_configuration_statement_add_action(s, "IAM:GetSettings");
        nm_iam_configuration_statement_add_action(s, "IAM:SetDeviceInfo");

        nm_iam_configuration_add_policy(conf, p);
    }

    {
        // Policy allowing management of own IAM user
        struct nm_iam_policy* p = nm_iam_configuration_policy_new("ManageOwnUser");
        {
            struct nm_iam_statement* s = nm_iam_configuration_policy_create_statement(p, NM_IAM_EFFECT_ALLOW);
            nm_iam_configuration_statement_add_action(s, "IAM:GetUser");
            nm_iam_configuration_statement_add_action(s, "IAM:DeleteUser");
            nm_iam_configuration_statement_add_action(s, "IAM:SetUserDisplayName");
            nm_iam_configuration_statement_add_action(s, "IAM:SetUserOauthSubject");
            nm_iam_configuration_statement_add_action(s, "IAM:SetUserPassword");

            // Create a condition such that only connections where the
            // UserId matches the UserId of the operation is allowed. E.g. IAM:Username == ${Connection:Username}

            struct nm_iam_condition* c = nm_iam_configuration_statement_create_condition(s, NM_IAM_CONDITION_OPERATOR_STRING_EQUALS, "IAM:Username");
            nm_iam_configuration_condition_add_value(c, "${Connection:Username}");
        }
        {
            struct nm_iam_statement* s = nm_iam_configuration_policy_create_statement(p, NM_IAM_EFFECT_ALLOW);
            nm_iam_configuration_statement_add_action(s, "IAM:ListUsers");
            nm_iam_configuration_statement_add_action(s, "IAM:ListRoles");
        }

        nm_iam_configuration_add_policy(conf, p);
    }

    {
        // Role allowing unpaired connections to pair
        struct nm_iam_role* r = nm_iam_configuration_role_new("Unpaired");
        nm_iam_configuration_role_add_policy(r, "Pairing");
        nm_iam_configuration_role_add_policy(r, "WebrtcSignaling");
        nm_iam_configuration_add_role(conf, r);
    }

    {
        // Role allowing everything assigned to administrators
        struct nm_iam_role* r = nm_iam_configuration_role_new("Administrator");
        nm_iam_configuration_role_add_policy(r, "ManageIam");
        nm_iam_configuration_role_add_policy(r, "Webrtc");
        nm_iam_configuration_role_add_policy(r, "Pairing");
        nm_iam_configuration_role_add_policy(r, "WebrtcSignaling");
        nm_iam_configuration_add_role(conf, r);
    }
    nm_iam_configuration_set_unpaired_role(conf, "Unpaired");

    char* jsonConfig = NULL;
    if (!nm_iam_serializer_configuration_dump_json(conf, &jsonConfig)) {
        nm_iam_configuration_free(conf);
        NPLOGE << "Failed to serialize IAM config";
        return false;
    }
    try {
        std::ofstream configFile(iamConfPath_);
        configFile << jsonConfig;
    }
    catch (std::exception& ex) {
        NPLOGE << "Failed to write to IAM config file: " << iamConfPath_ << " exception: " << ex.what();
        nm_iam_serializer_string_free(jsonConfig);
        nm_iam_configuration_free(conf);
        return false;
    }
    nm_iam_serializer_string_free(jsonConfig);
    nm_iam_configuration_free(conf);
    return true;
}

} // namespace
