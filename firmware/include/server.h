#pragma once

// esp_http_server is synchr, blocking, creates RTOS thread. simpler and less RAM
// ESPAsyncWebServer is async, nonblocking, better concurrency.
// TODO choose one 

#include "esp_http_server.h"
// #include <ESPAsyncWebServer.h>
#include "esp_camera.h"

httpd_handle_t server = NULL;
// AsyncWebServer server(80);

static esp_err_t handle_image(httpd_req_t* req);
static esp_err_t handle_stream(httpd_req_t* req);
// void handle_image(AsyncWebServerRequest* request);
// void handle_stream(AsyncWebServerRequest* request);

void init_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_uri_t image_uri = {
        .uri     = "/image",
        .method  = HTTP_GET,
        .handler = handle_image,
        .user_ctx = NULL
    };

    httpd_uri_t stream_uri = {
        .uri     = "/stream",
        .method  = HTTP_GET,
        .handler = handle_stream,
        .user_ctx = NULL
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &image_uri);
        httpd_register_uri_handler(server, &stream_uri);
        Serial.println("hosting http server successfully.");
    } else {
        Serial.println("unable to host http server.");
    }
}
// void init_server() {
//     server.on("/image", HTTP_GET, handle_image);
//     server.on("/stream", HTTP_GET, handle_stream);

//     server.onNotFound([](AsyncWebServerRequest* request) {
//         request->send(404, "text/plain", "404 - Not Found");
//     });

//     server.begin();
//     Serial.println("HTTP Server started");
// }

static esp_err_t handle_image(httpd_req_t *req) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera Capture failed!!");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    Serial.printf("someone requested a pic. Length: %d\n", fb->len);

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}
// void handle_image(AsyncWebServerRequest* request) {
//     camera_fb_t* fb = esp_camera_fb_get();
//     if (!fb) {
//         request->send(500, "text/plain", "Capture failed");
//         return;
//     }
//     Serial.printf("Took a pic. Length: %d\n", fb->len);
//     AsyncWebServerResponse* response = request->beginResponse(
//         "image/jpeg",
//         fb->len,
//         [fb](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
//             size_t remaining = fb->len - index;
//             size_t toSend = min(remaining, maxLen);
//             memcpy(buffer, fb->buf + index, toSend);
//             if (index + toSend == fb->len) {
//                 esp_camera_fb_return(fb);
//             }
//             return toSend;
//         }
//     );
//     response->addHeader("Access-Control-Allow-Origin", "*");
//     response->addHeader("Content-Disposition", "inline; filename=capture.jpg");
//     response->addHeader("Content-Length", String(fb->len));
//     response->setContentLength(fb->len);
//     request->send(response);
// }

static esp_err_t handle_stream(httpd_req_t *req) {
    Serial.println("Someone requested a video stream");
    camera_fb_t* fb = NULL;
    esp_err_t res = ESP_OK;
    char part_header[128];

    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=123456789000000000000987654321");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera Capture failed!!");
            res = ESP_FAIL;
            break;
        }

        // send frame boundary
        res = httpd_resp_send_chunk(req, "\r\n--123456789000000000000987654321\r\n", 36);
        if (res != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        size_t header_len = snprintf(
            part_header, sizeof(part_header),
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
            fb->len);

        // send header
        res = httpd_resp_send_chunk(req, part_header, header_len);
        if (res != ESP_OK) {
            esp_camera_fb_return(fb);
            break;
        }

        // send image binary
        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        esp_camera_fb_return(fb);
        if (res != ESP_OK) {
            break;
        }
    }

    httpd_resp_send_chunk(req, NULL, 0); // signal end-of-stream
    return res;
}
// void handle_stream(AsyncWebServerRequest* request) {
//     // todo
// }