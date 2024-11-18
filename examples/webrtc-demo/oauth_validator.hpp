#pragma once

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "nabto_device.hpp"
#include <util/util.hpp>

#include <nlohmann/json.hpp>
#include <curl/curl.h>
// jwt-cpp uses std::round it fails on some toolchains.
#if defined(NABTO_WEBRTC_CXX_STD_ROUND_FIX)
#include <util/cxx_std_round_fix.hpp>
#endif
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>
#include <openssl/param_build.h>

#include <memory>
#include <sstream>

namespace example {

class NabtoOauthValidator;

typedef std::shared_ptr<NabtoOauthValidator> NabtoOauthValidatorPtr;


class NabtoOauthValidator : public std::enable_shared_from_this<NabtoOauthValidator>
{
public:
    NabtoOauthValidator(std::string& url, std::string& issuer, std::string& productId, std::string& deviceId, const std::optional<std::string>& caBundle = std::nullopt)
        : url_(url), issuer_(issuer), productId_(productId), deviceId_(deviceId), caBundle_(caBundle)
    {

    }

    ~NabtoOauthValidator()
    {
    }

    bool validateToken(const std::string& token, std::function<void (bool valid, std::string subject)> cb)
    {

        auto self = shared_from_this();
        curl_ = nabto::CurlAsync::create();
        if (!curl_) {
            NPLOGE << "Failed to create CurlAsync object";
            return false;
        }
        if (!setupJwks()) {
            NPLOGE << "Failed to setup JWKS request";
            return false;
        }

        curl_->asyncInvoke([self, token, cb](CURLcode res, uint16_t statusCode) {

            NPLOGD << "    JWKS server returned status: " << statusCode;
            NPLOGD << "    Try parsing jwks_: " << self->jwks_;

            if (res != CURLE_OK || statusCode > 299 || statusCode < 200) {
                NPLOGE << "Curl failed with CURLcode: " << curl_easy_strerror(res) << " statusCode: " << statusCode;
                cb(false, "");
                return;
            }

            std::string subject;

            try {
                auto decoded_jwt = jwt::decode(token);
                auto jwks = jwt::parse_jwks(self->jwks_);
                auto jwk = jwks.get_jwk(decoded_jwt.get_key_id());

                NPLOGD << "    decoded JWT: " << decoded_jwt.get_payload();

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
                    cb(false, "");
                    NPLOGE << "Verification failed: " << ex.what();
                    return;
                }

                try {
                    NPLOGD << "    decoded again sub claim: " << decoded.get_payload_claim("sub").as_string();
                    subject = decoded.get_payload_claim("sub").as_string();
                } catch (std::exception& ex) {
                    NPLOGD << "    decoded claim did not contain a subject" << decoded.get_payload();
                    cb(false, "");
                    return;
                }
            } catch (std::invalid_argument& ex) {
                NPLOGE << "Invalid JWT format";
                cb(false, "");
                return;
            } catch (std::runtime_error& ex) {
                NPLOGE << "Invalid JWK/JSON format";
                cb(false, "");
                return;
            }
            NPLOGD << "Token valid!";
            // TODO: succeed
            cb(true, subject);
        });

        return true;

    }

    std::string createChallengeResponse(std::string& rawPrivateKey, std::string& clientFp, std::string& deviceFp, std::string& nonce)
    {
        // jwt::algorithm::es256 alg = es256algFromRawkey(rawPrivateKey);
        uint8_t decodedKey[32];

        nabto::fromHex(rawPrivateKey, decodedKey);

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
        NPLOGD << "EVP_PKEY_fromdata returned: " << i;

        std::string privateKey;
        std::string publicKey;

        BIOPtr bio(BIO_new(BIO_s_mem()));
        PEM_write_bio_PrivateKey(bio.get(), pkey, NULL, NULL, 0, NULL, NULL);

        char* data;
        long dataLength = BIO_get_mem_data(bio.get(), &data);
        privateKey = std::string(data, dataLength);
        NPLOGD << "PrivateKey: " << privateKey;

        // const EC_KEY* ecPubKey = EVP_PKEY_get0_EC_KEY(pkey);
        // const EC_POINT* ecPubPoint = EC_KEY_get0_public_key(ecPubKey);

        BN_CTX* bnCtx = BN_CTX_new();

        BIGNUM* x = BN_new();
        BIGNUM* y = BN_new();
        EC_GROUP* group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);

        EC_POINT* ecPubPoint = EC_POINT_new(group);
        i = EC_POINT_mul(group, ecPubPoint, k, nullptr, nullptr, nullptr);
        NPLOGD << "EC_POINT_mul returned: " << i;


        i = EC_POINT_get_affine_coordinates(group, ecPubPoint, x, y, bnCtx);
        NPLOGD << "Get affine coordinates returned: " << i;

        uint8_t xBin[32];
        i = BN_bn2bin(x, xBin);
        NPLOGD << "x bn2bin returned: " << i;

        uint8_t yBin[32];
        i = BN_bn2bin(y, yBin);
        NPLOGD << "y bn2bin returned: " << i;

        for (int i = 0; i < 32; i++) {
            NPLOGD << std::setfill('0') << std::setw(2) << std::hex << (int)yBin[i];
        }

        const std::string xStr = std::string((char*)xBin, 32);
        auto xEncoded = jwt::base::trim<jwt::alphabet::base64url>(jwt::base::encode<jwt::alphabet::base64url>(xStr));
        NPLOGD << "Base64 x: " << xEncoded;

        const std::string yStr = std::string((char*)yBin, 32);
        auto yEncoded = jwt::base::trim<jwt::alphabet::base64url>(jwt::base::encode<jwt::alphabet::base64url>(yStr));
        NPLOGD << "Base64 y: " << yEncoded;

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

        EC_POINT_free(ecPubPoint);
        EC_GROUP_free(group);
        BN_free(y);
        BN_free(x);
        BN_CTX_free(bnCtx);
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        OSSL_PARAM_free(params);
        OSSL_PARAM_BLD_free(param_bld);
        BN_free(k);


        return token;
    }

private:

    bool setupJwks()
    {
        CURLcode res = CURLE_OK;
        CURL* c = curl_->getCurl();

        NPLOGD << "Setting URL in curl: " << url_;
        res = curl_easy_setopt(c, CURLOPT_URL, url_.c_str());
        if (res != CURLE_OK) {
            NPLOGE << "Failed to set Curl URL option with: " << curl_easy_strerror(res);
            return false;
        }

        res = curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeFunc);
        if (res != CURLE_OK) {
            NPLOGE << "Failed to set Curl write function option with: " << curl_easy_strerror(res);
            return false;
        }

        res = curl_easy_setopt(c, CURLOPT_WRITEDATA, (void*)&jwks_);
        if (res != CURLE_OK) {
            NPLOGE << "Failed to set Curl write function option with: " << curl_easy_strerror(res);
            return false;
        }

        if (caBundle_.has_value()) {
            res = curl_easy_setopt(c, CURLOPT_CAINFO, caBundle_.value().c_str());
            if (res != CURLE_OK) {
                NPLOGE << "Failed to set CA bundle with: " << curl_easy_strerror(res);
                return false;
            }
        }

        return true;
    }

    template<typename json_traits>
    jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson>& getAllowedAlg(jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson>& verifier, const jwt::jwk<json_traits>& jwk)
    {
        if (jwk.get_key_type() == "RSA") {
            NPLOGD << "JWK key type was RSA";
            std::string rsaKey;
            if (!keyToRsa(jwk, rsaKey)) {
                NPLOGE << "keyToRsa failed";
                // cb(false, "");
                // TODO: fail
                return verifier;
            }
            return verifier.allow_algorithm(jwt::algorithm::rs256(rsaKey, "", "", ""));
        }
        else if (jwk.get_key_type() == "EC") {
            NPLOGD << "JWK key type was EC";

            std::string pubKey;
            if (!keyToEcdsa(jwk, pubKey)) {
                NPLOGE << "keyToEcdsa failed";
                // cb(false, "");
                // TODO: fail
                return verifier;
            }

            NPLOGD << "Got pubkey: " << pubKey;

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
        NPLOGD << "EVP_PKEY_fromdata returned: " << i;

        // TODO test status
        BIOPtr bio(BIO_new(BIO_s_mem()));


        i = PEM_write_bio_PUBKEY(bio.get(), pkey);
        NPLOGD << "PEM_write_bio_PUBKEY returned: " << i;

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
            NPLOGD << "s was stdout, this is header data";
            std::string data((char*)ptr, size * nmemb);
            NPLOGD << data;
            return size * nmemb;
        }
        try {
            ((std::string*)s)->append((char*)ptr, size * nmemb);
        }
        catch (std::exception& ex) {
            NPLOGE << "WriteFunc failure";
            return size * nmemb;
        }
        return size * nmemb;
    }

    std::string url_;
    std::string issuer_;
    std::string productId_;
    std::string deviceId_;
    std::optional<std::string> caBundle_;

    nabto::CurlAsyncPtr curl_ = nullptr;

    std::string jwks_;
};

} // namespace
