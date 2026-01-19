#include <curl/curl.h>
#include <getopt.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>

#define TIMEOUT 15L

static size_t discard_write(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    (void)ptr;
    (void)userdata;
    return size * nmemb;
}

struct location {
    char *json;
    size_t size;
};

struct myLocation{
    char *country;
    char *city;
    char *serverCity;
    char *host;
};

double download_test(const char *url)
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        printf("Failed to initialize CURL for download test\n");
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
        printf("Failed to initialize CURL for upload test\n");
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

static size_t save_location(char *data, size_t size, size_t nmemb, void *location)
{
    size_t realsize = size * nmemb;
    struct location *loc = (struct location *)location;
    char *ptr = realloc(loc->json, loc->size + realsize + 1);
    if(!ptr) return 0;

    loc->json = ptr;
    memcpy(&(loc->json[loc->size]), data, realsize);
    loc->size += realsize;
    loc->json[loc->size] = 0;

    return realsize;
}

void find_location(struct location *loc)
{
    CURL *curl = curl_easy_init();
    if(!curl){
        printf("Failed to initialize CURL for finding location\n");
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, "http://ip-api.com/json");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, save_location);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, loc);

    CURLcode res = curl_easy_perform(curl);

    if(res == !CURLE_OK){
        printf("CURL perform failed %s\n", curl_easy_strerror(res));
    }
    
    int code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    printf("returned code: %d\n", code);

    curl_easy_cleanup(curl);
}

cJSON* get_server_list()
{
    FILE *fp;
    fp = fopen("speedtest_server_list.json", "r");
    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    char data[size]; 
    fread(data, 1, size, fp);
    cJSON *servers = cJSON_Parse(data);
    fclose(fp);
    return servers;
}

void best_server_by_location(struct myLocation *mine, cJSON *servers)
{
    for(int i = 0; i<cJSON_GetArraySize(servers); i++)
    {
        cJSON *tmp = cJSON_GetArrayItem(servers, i);
        cJSON *country = cJSON_GetObjectItem(tmp, "country");
        cJSON *city = cJSON_GetObjectItem(tmp, "city");
        if(strcmp(city->valuestring, mine->city) == 0)
        {
            mine->host = cJSON_Print(cJSON_GetObjectItem(tmp, "host"));
            mine->serverCity = cJSON_Print(city);
            return;
        }
        if(strcmp(city->valuestring, mine->city) == 0)
        {
            mine->host = cJSON_Print(cJSON_GetObjectItem(tmp, "host"));
            mine->serverCity = cJSON_Print(city);
        }
    }
    if(mine->serverCity == NULL && mine->host == NULL)
    {
        printf("Could not find server in user country, defaulting to first server in list...");
        cJSON *first = cJSON_GetArrayItem(servers, 0);
        mine->serverCity = cJSON_Print(cJSON_GetObjectItem(first, "city"));
        mine->host = cJSON_Print(cJSON_GetObjectItem(first, "host"));
    }
}

int main(int argc, char **argv)
{
    const char *dl_test_url = "speedtest.a-mobile.biz:8080/speedtest/random4000x4000.jpg";
    const char *ul_test_url = "speedtest.a-mobile.biz:8080/speedtest/upload";
    
    //double speed_mbps = download_test(dl_test_url);
    //printf("Download speed: %.2f Mbps\n", speed_mbps);

    // double upload_mbps = upload_test(ul_test_url);
    // printf("Upload speed: %0.2f Mbps\n", upload_mbps);

    struct location *loc = calloc(1, sizeof(struct location));
    find_location(loc);
    
    cJSON *json = cJSON_Parse(loc->json);
    cJSON *country = cJSON_GetObjectItem(json, "country");
    cJSON *city = cJSON_GetObjectItem(json, "city");
    
    struct myLocation mine;
    mine.country = country->valuestring;
    mine.city = city->valuestring;

    if(mine.country != NULL && mine.city != NULL){
        printf("my Country: %s City: %s\n", mine.country, mine.city);
    }

    cJSON *servers = get_server_list();
    best_server_by_location(&mine, servers);
    printf("best server host(%s): %s\n", mine.serverCity, mine.host);

    // cJSON_Delete(json);
    // free(loc->json);
    // free(loc);
    
    return 0;
}