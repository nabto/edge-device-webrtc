#pragma once

#include "nabto_device.hpp"

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
        // TODO: consider if this should be reused to validate multiple tokens. If so, jwks should probably be retreived once in the beginning and cached (with refresh feature if it has expired).
        CURLcode res;
        res = curl_global_init(CURL_GLOBAL_ALL);
        if (res != CURLE_OK) {
            std::cout << "Failed to initialize Curl global" << std::endl;
            return;
        }
        curl_ = curl_easy_init();
        if (!curl_) {
            std::cout << "Failed to initialize Curl easy" << std::endl;
            return;
        }
    }

    ~NabtoOauthValidator()
    {
        curl_easy_cleanup(curl_);
        curl_global_cleanup();
    }

    void validateToken(const std::string& token, std::function<void (bool valid, std::string username)> cb)
    {
        // TODO: no Zalgo
        if (!setupCurl()) {
            cb(false, "");
            // TODO: fail
            return;
        }
        if (!getJwks()) {
            cb(false, "");
            // TODO: fail
            return;
        }

        auto decoded_jwt = jwt::decode(token);
        auto jwks = jwt::parse_jwks(jwks_);
        auto jwk = jwks.get_jwk(decoded_jwt.get_key_id());

        std::cout << "    decoded JWT: " << decoded_jwt.get_payload() << std::endl;

        std::string rsaKey;
        if (!keyToRsa(jwk, rsaKey)) {
            std::cout << "keyToRsa failed" << std::endl;
            cb(false, "");
            // TODO: fail
            return;
        }

        std::stringstream aud;
        aud << "nabto://device?productId=" << productId_ << "&deviceId=" << deviceId_;

        auto verifier =
            jwt::verify()
            .allow_algorithm(jwt::algorithm::rs256(rsaKey, "", "", ""))
            .with_issuer(issuer_)
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
        return;

    }

private:

    bool setupCurl() {
        CURLcode res;
        res = curl_easy_setopt(curl_, CURLOPT_VERBOSE, 0L);
        if (res != CURLE_OK) {
            std::cout << "Failed to set curl logging option" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L);
        if (res != CURLE_OK) {
            std::cout << "Failed to set Curl progress option" << std::endl;
            return false;
        }
        res = curl_easy_setopt(curl_, CURLOPT_HEADERDATA, stdout);
        if (res != CURLE_OK) {
            std::cout << "Failed to set Curl header data option" << std::endl;
            return false;
        }
        return true;
    }

    bool getJwks()
    {
        // TODO: dont use blocking curl in coap request callback. Switch to curl_multi_perform()
        CURLcode res = CURLE_OK;

        res = curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());
        if (res != CURLE_OK) {
            std::cout << "Failed to set Curl URL option" << std::endl;
            return false;
        }

        res = curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeFunc);
        if (res != CURLE_OK) {
            std::cout << "Failed to set Curl write function option" << std::endl;
            return false;
        }

        res = curl_easy_setopt(curl_, CURLOPT_WRITEDATA, (void*)&jwks_);
        if (res != CURLE_OK) {
            std::cout << "Failed to set Curl write function option" << std::endl;
            return false;
        }


        res = curl_easy_perform(curl_);
        if (res != CURLE_OK) {
            std::cout << "Failed to perform JWKS request" << std::endl;
            return false;
        }

/*
Got JWKS response:
{
    "keys":[
        {
            "kty":"RSA",
            "use":"sig",
            "kid":"keystore-CHANGE-ME",
            "alg":"RS256",
            "e":"AQAB",
            "n":"xwQ72P9z9OYshiQ-ntDYaPnnfwG6u9JAdLMZ5o0dmjlcyrvwQRdoFIKPnO65Q8mh6F_LDSxjxa2Yzo_wdjhbPZLjfUJXgCzm54cClXzT5twzo7lzoAfaJlkTsoZc2HFWqmcri0BuzmTFLZx2Q7wYBm0pXHmQKF0V-C1O6NWfd4mfBhbM-I1tHYSpAMgarSm22WDMDx-WWI7TEzy2QhaBVaENW9BKaKkJklocAZCxk18WhR0fckIGiWiSM5FcU1PY2jfGsTmX505Ub7P5Dz75Ygqrutd5tFrcqyPAtPTFDk8X1InxkkUwpP3nFU5o50DGhwQolGYKPGtQ-ZtmbOfcWQ"
        }
    ]
}
*/
        std::cout << "Got JWKS response: " << jwks_ << std::endl;
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

    CURL* curl_;

    std::string jwks_;
};

} // namespace
