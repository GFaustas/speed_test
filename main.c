#include <curl/curl.h>
#include <getopt.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <memory.h>

#define TIMEOUT 15L

static size_t discard_write(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    (void)ptr;
    (void)userdata;
    return size * nmemb;
}

double download_test(const char *url)
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        printf("Failed to initialize CURL\n");
        return 0;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_write);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        printf("CURL perform failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return 0;
    }
    int code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    printf("returned code: %d\n", code);
    curl_off_t downloaded = 0;

    curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD_T, &downloaded);
    if(!downloaded) {
        printf("No data downloaded\n");
        curl_easy_cleanup(curl);
        return 0;
    }
    curl_easy_cleanup(curl);
    return (downloaded * 8.0) / 1000000;
}

double upload_test(const char *url)
{
    CURL *curl = curl_easy_init();
    if(!curl)
    {
        printf("Failed to initialize CURL\n");
        return 0;
    }

    const size_t payload_size = 100 * 1024 * 1024;
    char *payload = malloc(payload_size);
    memset(payload, 'A', payload_size);
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload_size);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_write);
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        printf("CURL perform failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return 0;
    }

    int code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    printf("returned code: %d\n", code);
    
    curl_off_t uploaded = 0;
    curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD_T, &uploaded);
    if(!uploaded) {
        printf("No data uploaded\n");
        curl_easy_cleanup(curl);
        return 0;
    }

    free(payload);
    curl_easy_cleanup(curl);
    return (uploaded * 8.0) / (1000000);
}

int main(int argc, char **argv)
{
    const char *dl_test_url = "speedtest.a-mobile.biz:8080/speedtest/random4000x4000.jpg";
    const char *ul_test_url = "speedtest.a-mobile.biz:8080/speedtest/upload";
    //double speed_mbps = download_test(dl_test_url);
    //printf("Download speed: %.2f Mbps\n", speed_mbps);
    double upload_mbps = upload_test(ul_test_url);
    printf("Upload speed: %0.2f Mbps\n", upload_mbps);
    return 0;
}