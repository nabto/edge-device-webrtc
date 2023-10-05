#pragma once

#include "nabto_device.hpp"
#include <util.hpp>

#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <jwt-cpp/jwt.h>
#include <openssl/param_build.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>

#include <memory>

namespace nabto {

class NabtoOauthValidator;

typedef std::shared_ptr<NabtoOauthValidator> NabtoOauthValidatorPtr;


class NabtoOauthValidator : public std::enable_shared_from_this<NabtoOauthValidator>
{
public:
    NabtoOauthValidator(std::string& url, std::string& issuer, std::string& productId, std::string& deviceId)
        : url_(url), issuer_(issuer), productId_(productId), deviceId_(deviceId)
    {

    }

    ~NabtoOauthValidator()
    {
    }

    bool validateToken(const std::string& token, std::function<void (bool valid, std::string subject)> cb)
    {

        auto self = shared_from_this();
        curl_ = CurlAsync::create();
        if (!curl_) {
            std::cout << "Failed to create CurlAsync object" << std::endl;
            return false;
        }
        if (!setupJwks()) {
            std::cout << "Failed to setup JWKS request" << std::endl;
            return false;
        }

        curl_->asyncInvoke([self, token, cb](CURLcode res) {

            std::cout << "    Try parsing jwks_: " << self->jwks_ << std::endl;
            auto decoded_jwt = jwt::decode(token);
            auto jwks = jwt::parse_jwks(self->jwks_);
            auto jwk = jwks.get_jwk(decoded_jwt.get_key_id());

            std::cout << "    decoded JWT: " << decoded_jwt.get_payload() << std::endl;

            std::stringstream aud;
            aud << "nabto://device?productId=" << self->productId_ << "&deviceId=" << self->deviceId_;

            auto verifier =
                jwt::verify();

            verifier = self->getAllowedAlg(verifier, jwk)
                .with_issuer(self->issuer_)
                .with_audience(aud.str())
                .leeway(60UL); // value in seconds, add some to compensate timeout
            auto decoded = jwt::decode(token);
            try {
                verifier.verify(decoded);
            } catch (jwt::error::token_verification_exception& ex) {
                // TODO: fail
                cb(false, "");
                std::cout << "Verification failed: " << ex.what() << std::endl;
                return;
            }
            std::string subject;
            try {
                // TODO: switch to claim `sub` when we have a proper oidc server
                std::cout << "    decoded again sub claim: " << decoded.get_payload_claim("sub") << std::endl;
                subject = decoded.get_payload_claim("sub").as_string();
            } catch (std::exception& ex) {
                std::cout << "    decoded claim did not contain a subject" << decoded.get_payload() << std::endl;
                cb(false, "");
                return;
            }

            std::cout << "Token valid!" << std::endl;
            // TODO: succeed
            cb(true, subject);
        });

        return true;

    }

    std::string createChallengeResponse(std::string& rawPrivateKey, std::string& clientFp, std::string& deviceFp, std::string& nonce)
    {
        // jwt::algorithm::es256 alg = es256algFromRawkey(rawPrivateKey);
        uint8_t decodedKey[32];

        fromHex(rawPrivateKey, decodedKey);
        std::cout << "Decoded rawKey: " << rawPrivateKey << " to: ";

        for (int i = 0; i < 32; i++) {
            std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)decodedKey[i];
        }
        std::cout << std::dec << std::endl;

        BIGNUM* k = BN_bin2bn(reinterpret_cast<const uint8_t*>(decodedKey), 32, NULL);

        EVP_PKEY_CTX* ctx;
        EVP_PKEY* pkey = NULL;
        OSSL_PARAM_BLD* param_bld;
        OSSL_PARAM* params = NULL;

        param_bld = OSSL_PARAM_BLD_new();
        OSSL_PARAM_BLD_push_utf8_string(param_bld, "group", "prime256v1", 0);
        OSSL_PARAM_BLD_push_BN(param_bld, "priv", k);
        OSSL_PARAM_BLD_push_int(param_bld, "include-public", 0);

        params = OSSL_PARAM_BLD_to_param(param_bld);
        ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
        EVP_PKEY_fromdata_init(ctx);
        auto i = EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_KEYPAIR, params);
        std::cout << "EVP_PKEY_fromdata returned: " << i << std::endl;

        std::string privateKey;
        std::string publicKey;

        BIOPtr bio(BIO_new(BIO_s_mem()));
        PEM_write_bio_PrivateKey(bio.get(), pkey, NULL, NULL, 0, NULL, NULL);

        char* data;
        long dataLength = BIO_get_mem_data(bio.get(), &data);
        privateKey = std::string(data, dataLength);
        std::cout << "PrivateKey: " << privateKey << std::endl;

        // const EC_KEY* ecPubKey = EVP_PKEY_get0_EC_KEY(pkey);
        // const EC_POINT* ecPubPoint = EC_KEY_get0_public_key(ecPubKey);

        BN_CTX* bnCtx = BN_CTX_new();

        BIGNUM* x = BN_new();
        BIGNUM* y = BN_new();
        EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);

        EC_POINT* ecPubPoint = EC_POINT_new(group);
        i = EC_POINT_mul(group, ecPubPoint, k, nullptr, nullptr, nullptr);
        std::cout << "EC_POINT_mul returned: " << i << std::endl;


        i = EC_POINT_get_affine_coordinates(group, ecPubPoint, x, y, bnCtx);
        std::cout << "Get affine coordinates returned: " << i << std::endl;

        uint8_t xBin[32];
        i = BN_bn2bin(x, xBin);
        std::cout << "x bn2bin returned: " << i << std::endl;

        uint8_t yBin[32];
        i = BN_bn2bin(y, yBin);
        std::cout << "y bn2bin returned: " << i << std::endl;

        for (int i = 0; i < 32; i++) {
            std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)yBin[i];
        }

        const std::string xStr = std::string((char*)xBin, 32);
        auto xEncoded = jwt::base::trim<jwt::alphabet::base64url>(jwt::base::encode<jwt::alphabet::base64url>(xStr));
        std::cout << "Base64 x: " << xEncoded << std::endl;

        const std::string yStr = std::string((char*)yBin, 32);
        auto yEncoded = jwt::base::trim<jwt::alphabet::base64url>(jwt::base::encode<jwt::alphabet::base64url>(yStr));
        std::cout << "Base64 y: " << yEncoded << std::endl;

        jwt::algorithm::es256 alg = jwt::algorithm::es256("", privateKey, "", "");


        auto jwkKey = nlohmann::json{
            {"kty", "EC"},
            {"crv", "P-256"},
            {"x", xEncoded},
            {"y", yEncoded}
        };
        auto token = jwt::create<jwt::traits::nlohmann_json>()
        .set_type("JWS")
            .set_header_claim("jwk", jwt::basic_claim<jwt::traits::nlohmann_json>(jwkKey))
            .set_payload_claim("client_fingerprint", jwt::basic_claim<jwt::traits::nlohmann_json>(clientFp))
            .set_payload_claim("device_fingerprint", jwt::basic_claim<jwt::traits::nlohmann_json>(deviceFp))
            .set_payload_claim("nonce", jwt::basic_claim<jwt::traits::nlohmann_json>(nonce))
            .sign(alg);
        ;
        // TODO: cleanup openssl stuff properly
        return token;
    }

private:

    bool setupJwks()
    {
        CURLcode res = CURLE_OK;
        CURL* c = curl_->getCurl();

        std::cout << "Setting URL in curl: " << url_ << std::endl;
        res = curl_easy_setopt(c, CURLOPT_URL, url_.c_str());
        if (res != CURLE_OK) {
            std::cout << "Failed to set Curl URL option" << std::endl;
            return false;
        }

        res = curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeFunc);
        if (res != CURLE_OK) {
            std::cout << "Failed to set Curl write function option" << std::endl;
            return false;
        }

        res = curl_easy_setopt(c, CURLOPT_WRITEDATA, (void*)&jwks_);
        if (res != CURLE_OK) {
            std::cout << "Failed to set Curl write function option" << std::endl;
            return false;
        }
        return true;
    }

    template<typename json_traits>
    jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson>& getAllowedAlg(jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson>& verifier, const jwt::jwk<json_traits>& jwk)
    {
        if (jwk.get_key_type() == "RSA") {
            std::cout << "JWK key type was RSA" << std::endl;
            std::string rsaKey;
            if (!keyToRsa(jwk, rsaKey)) {
                std::cout << "keyToRsa failed" << std::endl;
                // cb(false, "");
                // TODO: fail
                return verifier;
            }
            return verifier.allow_algorithm(jwt::algorithm::rs256(rsaKey, "", "", ""));
        }
        else if (jwk.get_key_type() == "EC") {
            std::cout << "JWK key type was EC" << std::endl;

            std::string pubKey;
            if (!keyToEcdsa(jwk, pubKey)) {
                std::cout << "keyToEcdsa failed" << std::endl;
                // cb(false, "");
                // TODO: fail
                return verifier;
            }

            std::cout << "Got pubkey: " << pubKey << std::endl;

            return verifier.allow_algorithm(jwt::algorithm::es256(pubKey, "", "", ""));
        }
        return verifier;
    }

    struct BIOFree {
        void operator()(BIO* bio) {
            BIO_free(bio);
        }
    };

    typedef std::unique_ptr<BIO, BIOFree> BIOPtr;

    template<typename json_traits>
    bool keyToRsa(const jwt::jwk<json_traits>& jwk, std::string& key)
    {

        if (!jwk.has_jwk_claim("n") || !jwk.has_jwk_claim("e")) {
            return false;
        }

        auto nClaim = jwk.get_jwk_claim("n").as_string();
        auto eClaim = jwk.get_jwk_claim("e").as_string();

        auto decodedN = jwt::base::decode<jwt::alphabet::base64url>(jwt::base::pad<jwt::alphabet::base64url>(nClaim));
        auto decodedE = jwt::base::decode<jwt::alphabet::base64url>(jwt::base::pad<jwt::alphabet::base64url>(eClaim));

        BIGNUM* n = BN_bin2bn(reinterpret_cast<const uint8_t*>(decodedN.c_str()), decodedN.size(), NULL);
        BIGNUM* e = BN_bin2bn(reinterpret_cast<const uint8_t*>(decodedE.c_str()), decodedE.size(), NULL);

        EVP_PKEY_CTX* ctx;
        EVP_PKEY* pkey = NULL;
        OSSL_PARAM_BLD* param_bld;
        OSSL_PARAM* params = NULL;

        param_bld = OSSL_PARAM_BLD_new();
        OSSL_PARAM_BLD_push_BN(param_bld, "n", n);
        OSSL_PARAM_BLD_push_BN(param_bld, "e", e);
        params = OSSL_PARAM_BLD_to_param(param_bld);
        ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
        EVP_PKEY_fromdata_init(ctx);
        EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_KEYPAIR, params);

        // TODO test status
        BIOPtr bio(BIO_new(BIO_s_mem()));


        PEM_write_bio_PUBKEY(bio.get(), pkey);

        BN_free(n);
        BN_free(e);
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(params);
        OSSL_PARAM_BLD_free(param_bld);

        char* data;
        long dataLength = BIO_get_mem_data(bio.get(), &data);
        std::string ret = std::string(data, dataLength);
        key = ret;
        return true;
    }

    template<typename json_traits>
    bool keyToEcdsa(const jwt::jwk<json_traits>& jwk, std::string& key)
    {
        if (!jwk.has_jwk_claim("x") || !jwk.has_jwk_claim("y")) {
            return false;
        }

        auto xClaim = jwk.get_jwk_claim("x").as_string();
        auto yClaim = jwk.get_jwk_claim("y").as_string();

        auto decodedX = jwt::base::decode<jwt::alphabet::base64url>(jwt::base::pad<jwt::alphabet::base64url>(xClaim));
        auto decodedY = jwt::base::decode<jwt::alphabet::base64url>(jwt::base::pad<jwt::alphabet::base64url>(yClaim));

        uint8_t publicKeyXy[1+32+32] = {0};
        publicKeyXy[0] = 4; // POINT_CONVERSION_UNCOMPRESSED
        memcpy(publicKeyXy + 1, decodedX.c_str(), 32);
        memcpy(publicKeyXy + 33, decodedY.c_str(), 32);

        EVP_PKEY_CTX* ctx;
        EVP_PKEY* pkey = NULL;
        OSSL_PARAM_BLD* param_bld;
        OSSL_PARAM* params = NULL;

        param_bld = OSSL_PARAM_BLD_new();
        OSSL_PARAM_BLD_push_utf8_string(param_bld, "group", "P-256", 5);
        OSSL_PARAM_BLD_push_octet_string(param_bld, "pub", publicKeyXy, 65);
        params = OSSL_PARAM_BLD_to_param(param_bld);
        ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
        EVP_PKEY_fromdata_init(ctx);
        auto i = EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_KEYPAIR, params);
        std::cout << "EVP_PKEY_fromdata returned: " << i << std::endl;

        // TODO test status
        BIOPtr bio(BIO_new(BIO_s_mem()));


        i = PEM_write_bio_PUBKEY(bio.get(), pkey);
        std::cout << "PEM_write_bio_PUBKEY returned: " << i << std::endl;

        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(params);
        OSSL_PARAM_BLD_free(param_bld);

        char* data;
        long dataLength = BIO_get_mem_data(bio.get(), &data);
        std::string ret = std::string(data, dataLength);
        key = ret;
        return true;

    }

    static size_t writeFunc(void* ptr, size_t size, size_t nmemb, void* s)
    {
        if (s == stdout) {
            std::cout << "s was stdout, this is header data";
            std::string data((char*)ptr, size * nmemb);
            std::cout << data;
            return size * nmemb;
        }
        try {
            ((std::string*)s)->append((char*)ptr, size * nmemb);
        }
        catch (std::exception& ex) {
            std::cout << "WriteFunc failure" << std::endl;
            return size * nmemb;
        }
        return size * nmemb;
    }

    bool fromHex(std::string& hex, uint8_t* data)
    {
        // hexLength should be 2*datalength or (2*dataLength - 1)
        size_t dataLength = hex.size()/2;
        memset(data, 0, dataLength);

        size_t index = 0;

        const char* end = hex.data() + hex.size();
        const char* ptr = hex.data();

        while (ptr < end) {
            char c = *ptr;
            uint8_t value = 0;
            if (c >= '0' && c <= '9')
                value = (c - '0');
            else if (c >= 'A' && c <= 'F')
                value = (10 + (c - 'A'));
            else if (c >= 'a' && c <= 'f')
                value = (10 + (c - 'a'));
            else {
                return false;
            }

            // shift each even hex byte 4 up
            data[(index / 2)] += value << (((index + 1) % 2) * 4);

            index++;
            ptr++;
        }

        return true;
    }

    std::string url_;
    std::string issuer_;
    std::string productId_;
    std::string deviceId_;

    CurlAsyncPtr curl_ = nullptr;

    std::string jwks_;
};

} // namespace
