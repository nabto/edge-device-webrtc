#pragma once

#include "nabto_device.hpp"
#include <util.hpp>

#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <jwt-cpp/jwt.h>

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

    bool validateToken(const std::string& token, std::function<void (bool valid, std::string username)> cb)
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

            auto decoded_jwt = jwt::decode(token);
            auto jwks = jwt::parse_jwks(self->jwks_);
            auto jwk = jwks.get_jwk(decoded_jwt.get_key_id());

            std::cout << "    decoded JWT: " << decoded_jwt.get_payload() << std::endl;

            std::string rsaKey;
            if (!self->keyToRsa(jwk, rsaKey)) {
                std::cout << "keyToRsa failed" << std::endl;
                cb(false, "");
                // TODO: fail
                return;
            }

            std::stringstream aud;
            aud << "nabto://device?productId=" << self->productId_ << "&deviceId=" << self->deviceId_;

            auto verifier =
                jwt::verify()
                .allow_algorithm(jwt::algorithm::rs256(rsaKey, "", "", ""))
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

            std::cout << "Token valid!" << std::endl;
            // TODO: succeed
            // TODO: username from claim
            cb(true, "admin");
        });

        return true;

    }

private:

    bool setupJwks()
    {
        CURLcode res = CURLE_OK;
        CURL* c = curl_->getCurl();

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

    // TODO: update to non-deprecated OpenSSL functions
    struct RSAFree {
        void operator()(RSA* rsa) {
            RSA_free(rsa);
        }
    };

    struct BNFree {
        void operator()(BIGNUM* bn) {
            BN_free(bn);
        }
    };

    struct BIOFree {
        void operator()(BIO* bio) {
            BIO_free(bio);
        }
    };

    struct EVP_PKEYFree {
        void operator()(EVP_PKEY* pkey) {
            EVP_PKEY_free(pkey);
        }
    };

    typedef std::unique_ptr<RSA, RSAFree> RSAPtr;
    typedef std::unique_ptr<BIGNUM, BNFree> BNPtr;
    typedef std::unique_ptr<BIO, BIOFree> BIOPtr;
    typedef std::unique_ptr<EVP_PKEY, EVP_PKEYFree> EVP_PKEYPtr;

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

        BNPtr n(BN_bin2bn(reinterpret_cast<const uint8_t*>(decodedN.c_str()), decodedN.size(), NULL));
        BNPtr e(BN_bin2bn(reinterpret_cast<const uint8_t*>(decodedE.c_str()), decodedE.size(), NULL));

        // TODO: this somehow causes memory leaks. It will likely be fixed when updating to non-deprecated usage
        RSAPtr rsa(RSA_new());

        RSA_set0_key(rsa.get(), n.release(), e.release(), NULL);

        EVP_PKEYPtr pkey(EVP_PKEY_new());

        int status = EVP_PKEY_set1_RSA(pkey.get(), rsa.release());
        // TODO test status
        BIOPtr bio(BIO_new(BIO_s_mem()));


        PEM_write_bio_PUBKEY(bio.get(), pkey.get());

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

    std::string url_;
    std::string issuer_;
    std::string productId_;
    std::string deviceId_;

    CurlAsyncPtr curl_ = nullptr;

    std::string jwks_;
};

} // namespace
