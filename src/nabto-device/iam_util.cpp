#include "nabto_device.hpp"

#include <modules/iam/nm_iam_serializer.h>

namespace nabto {

const std::string defaultState = R"({
  "Version": 1,
  "OpenPairingPassword": "demoOpenPairing",
  "OpenPairingSct": "demosct",
  "LocalOpenPairing": true,
  "PasswordOpenPairing": true,
  "PasswordInvitePairing": true,
  "LocalInitialPairing": true,
  "OpenPairingRole": "Administrator",
  "InitialPairingUsername": "admin",
  "FriendlyName": "Webrtc demo example",
  "Users": [
    {
      "Username": "admin",
      "ServerConnectToken": "demosct",
      "Role": "Administrator",
      "Password": "demoAdminPwd"
    }
  ]
})";

bool NabtoDeviceImpl::createDefaultIamState()
{
    try {
        auto jsonState = nlohmann::json::parse(defaultState);
        std::ofstream stateFile(iamStatePath_);
        stateFile << jsonState;
    }
    catch (std::exception& ex) {
        std::cout << "Failed to write to state file: " << iamStatePath_ << " exception: " << ex.what() << std::endl;
        return false;
    }
    return true;
}

bool NabtoDeviceImpl::createDefaultIamConfig()
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
        std::cout << "Failed to serialize IAM config" << std::endl;
        return false;
    }
    try {
        std::ofstream configFile(iamConfPath_);
        configFile << jsonConfig;
    }
    catch (std::exception& ex) {
        std::cout << "Failed to write to IAM config file: " << iamConfPath_ << " exception: " << ex.what() << std::endl;
        return false;
    }
    return true;
}

} // namespace
