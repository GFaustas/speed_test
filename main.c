#include <curl/curl.h>
#include <getopt.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define TIMEOUT 15L
#define DL_PATH "/speedtest/random4000x4000.jpg"
#define UL_PATH "/speedtest/upload"

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

void get_server_list(cJSON **servers)
{
    FILE *fp;
    fp = fopen("speedtest_server_list.json", "r");
    
    if(fp == NULL)
    {
        perror("Nepavyko atidaryti serverio sąrašo\n");
        exit(EXIT_FAILURE);
    }

    fseek(fp, 0L, SEEK_END);
    long size = ftell(fp);

    if(size == 0)
    {
        fprintf(stderr, "Serverių sąrašas yra tuščias\n");
        exit(EXIT_FAILURE);
    }

    rewind(fp);
    char *data = malloc(size + 1);
    fread(data, 1, size, fp);
    *servers = cJSON_Parse(data);
    free(data);

    if(*servers == NULL)
    {
        fprintf(stderr, "Nepavyko serverio sąrašo išsaugoti\n");
    }
    fclose(fp);
}

char* get_url(cJSON *servers, char *id)
{
    int wanted_id = atoi(id);
    printf("Gaunamas serveris pagal 'id'...\n");
    for(int i=0; i<cJSON_GetArraySize(servers); i++)
    {
        cJSON *tmp = cJSON_GetArrayItem(servers, i);
        if(cJSON_GetObjectItem(tmp, "id")->valueint == wanted_id)
        {
            return cJSON_GetObjectItem(tmp, "host")->valuestring;
        }
    }
    return NULL;
}

void download_test(char *url)
{
    printf("Vykdomas serverio %s parsiuntimo greičio nustatymas--------------------------------\n", url);
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "Nepavyko inicializuoti CURL duomenų parsisiuntimo testui\n");
        exit(EXIT_FAILURE);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_write);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        printf("CURL vykdymo klaida: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        exit(EXIT_FAILURE);
    }
    int code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    printf("Serverio grąžintas atsako kodas: %d\n", code);
    curl_off_t downloaded = 0;

    curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD_T, &downloaded);
    if(downloaded == 0) {
        fprintf(stderr, "Nepavyko parsisiųsti duomenų\n");
        curl_easy_cleanup(curl);
        exit(EXIT_FAILURE);
    }
    curl_easy_cleanup(curl);
    printf("Parsiuntimo greitis: %.2f Mbps\n", (downloaded * 8.0) / 1000000);
}

void upload_test(char *url)
{
    printf("Vykdomas serverio %s išsiuntimo greičio nustatymas--------------------------------\n", url);
    CURL *curl = curl_easy_init();
    if(curl == NULL)
    {
        fprintf(stderr, "Nepavyko inicializuoti CURL duomenų parsisiuntimo testui\n");
        exit(EXIT_FAILURE);
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
        printf("CURL vykdymo klaida: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        exit(EXIT_FAILURE);
    }

    int code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    printf("Serverio grąžintas atsako kodas: %d\n", code);
    
    curl_off_t uploaded = 0;
    curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD_T, &uploaded);
    if(uploaded == 0) {
        fprintf(stderr, "Nepavyko įkelti duomenų\n");
        curl_easy_cleanup(curl);
        exit(EXIT_FAILURE);
    }

    free(payload);
    curl_easy_cleanup(curl);
    printf("Įkėlimo greitis: %0.2f Mbps\n", (uploaded * 8.0) / (1000000));
}

char *build_url(const char *host, const char *path)
{
    if (!host || !path) return NULL;

    size_t len = strlen(host) + strlen(path) + 1;
    char *url = malloc(len);
    if (!url) return NULL;

    snprintf(url, len, "%s%s", host, path);
    return url;
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
    if(curl == NULL){
        fprintf(stderr, "Nepavyko inicializuoti CURL duomenų parsisiuntimo testui\n");
        exit(EXIT_FAILURE);
    }

    curl_easy_setopt(curl, CURLOPT_URL, "http://ip-api.com/json");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, save_location);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, loc);

    CURLcode res = curl_easy_perform(curl);

    if(res != CURLE_OK){
        printf("CURL vykdymo klaida: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        exit(EXIT_FAILURE);
    }
    
    int code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    printf("Serverio grąžintas atsako kodas: %d\n", code);

    curl_easy_cleanup(curl);
}

void find_location_handler(struct myLocation *mine)
{
    printf("Atliekamas naudotojo vietovės nustatymas--------------------------------\n");
    struct location *loc = calloc(1, sizeof(struct location));
    find_location(loc);

    printf("Išsaugoma lokacijos informacija...\n");
    cJSON *json = cJSON_Parse(loc->json);
    mine->country = strdup(cJSON_GetObjectItem(json, "country")->valuestring);
    mine->city = strdup(cJSON_GetObjectItem(json, "city")->valuestring);
    printf("Naudotojas yra: %s, %s\n", mine->country, mine->city);

    free(loc->json);
    free(loc);
    cJSON_Delete(json);
}

void best_server_by_location(struct myLocation *mine, cJSON *servers)
{
    printf("Atliekamas geriausio serverio, pagal naudotojo vietovę, nustatymas--------------------------------\n");
    for(int i = 0; i<cJSON_GetArraySize(servers); i++)
    {
        cJSON *tmp = cJSON_GetArrayItem(servers, i);
        cJSON *country = cJSON_GetObjectItem(tmp, "country");
        cJSON *city = cJSON_GetObjectItem(tmp, "city");
        if(city && mine->city && strcmp(city->valuestring, mine->city) == 0)
        {
            mine->host = strdup(cJSON_GetObjectItem(tmp, "host")->valuestring);
            mine->serverCity = strdup(city->valuestring);
            printf("Naudotojo geriausias serveris pagal lokacija yra: %s\n Serverio adresas: %s\n", mine->serverCity, mine->host);
            return;
        }
        if(country && mine->country && strcmp(country->valuestring, mine->country) == 0)
        {
            mine->host = strdup(cJSON_GetObjectItem(tmp, "host")->valuestring);
            mine->serverCity = strdup(city->valuestring);
        }
    }
    if(!mine->host)
    {
        printf("Naudotojo šalyje serveris nerastas, nustatomas numatytas serveris...\n");
        cJSON *first = cJSON_GetArrayItem(servers, 0);
        mine->city = strdup(cJSON_GetObjectItem(first, "city")->valuestring);
        mine->host = strdup(cJSON_GetObjectItem(first, "host")->valuestring);
    }
    printf("Naudotojo geriausias serveris pagal lokacija yra: %s\n Serverio adresas: %s\n", mine->serverCity, mine->host);
}

void auto_handler(struct myLocation *mine, cJSON *servers)
{
    find_location_handler(mine);
    best_server_by_location(mine, servers);
    char *durl = build_url(mine->host, DL_PATH);
    if (!durl) {
        fprintf(stderr, "Nepavyko sukurti URL\n");
        exit(EXIT_FAILURE);
    }
    download_test(durl);
    free(durl);
    char *uurl = build_url(mine->host, UL_PATH);
    if (!uurl) {
        fprintf(stderr, "Nepavyko sukurti URL\n");
        exit(EXIT_FAILURE);
    }
    upload_test(uurl);
    free(uurl);
}

int main(int argc, char *argv[])
{
    cJSON *servers = NULL;
    get_server_list(&servers);
    struct myLocation *mine = calloc(1, sizeof(*mine));    
    int opt;

    printf("----------------------------------------------------------------\n"
           "---------------------Naudojimosi instrukcija--------------------\n"
           "./main [-a (paleisti automatinius testus)\n" 
           "| -u 'id'(testuoti tam tikro, pagal 'id', serverio išsiuntimo greitį)\n"
           "| -d 'id'(testuoti tam tikro, pagal 'id', serverio parsiuntimo greitį)\n"
           "| -f (rasti geriausia serverį priklausomai nuo naudotojo vietovės)\n"
           "| -l (rasti naudotojo vietovę) ]\n"
           "----------------------------------------------------------------\n"
        );

    while((opt = getopt(argc, argv, "au:d:lf")) != -1)
    {
        switch (opt)
        {
            case 'a' :
                printf("Vykdomas automatinis testas--------------------------------\n");
                auto_handler(mine, servers);
                break;
            
            case 'u' :
                char *uhost = get_url(servers, optarg);
                if (!uhost) {
                    fprintf(stderr, "Serveris nerastas\n");
                    break;
                }

                char *uurl = build_url(uhost, UL_PATH);
                free(uhost);
                if (!uurl) {
                    fprintf(stderr, "Nepavyko sukurti URL\n");
                    break;
                }

                upload_test(uurl);
                free(uurl);
                break;
            
            case 'd' :
                char *dhost = get_url(servers, optarg);
                if (!dhost) {
                    fprintf(stderr, "Serveris nerastas\n");
                    break;
                }

                char *durl = build_url(dhost, DL_PATH);
                free(dhost);
                if (!durl) {
                    fprintf(stderr, "Nepavyko sukurti URL\n");
                    break;
                }

                download_test(durl);
                free(durl);
                break;
                
            case 'l' :
                find_location_handler(mine);
                break;
                
            case 'f' :
                if(mine->country == NULL && mine->city == NULL)
                {
                    find_location_handler(mine);
                }
                best_server_by_location(mine, servers);
                break;
                
            case '?':
                printf("Neteisinga komanda\n");
                break;
        }
    }
    cJSON_Delete(servers);
    free(mine->city);
    free(mine->serverCity);
    free(mine->host);
    free(mine->country);
    free(mine);
    return EXIT_SUCCESS;
}