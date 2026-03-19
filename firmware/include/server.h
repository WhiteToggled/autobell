#pragma once

// esp_http_server is synchr, blocking, creates RTOS thread. simpler and less RAM
// ESPAsyncWebServer is async, nonblocking, better concurrency. (v)
// TODO choose one 

// #include "esp_http_server.h"
#include <ESPAsyncWebServer.h>
#include "esp_camera.h"

// httpd_handle_t server = NULL;
AsyncWebServer server(80);

// static esp_err_t handle_image(httpd_req_t* req);
// static esp_err_t handle_stream(httpd_req_t* req);
void handle_image(AsyncWebServerRequest* request);
void handle_stream(AsyncWebServerRequest* request);

void init_server() {
    server.on("/image", HTTP_GET, handle_image);
    server.on("/stream", HTTP_GET, handle_stream);

    server.onNotFound([](AsyncWebServerRequest* request) {
        request->send(404, "text/plain", "404 - Not Found");
    });

    server.begin();
    Serial.println("HTTP Server started");
}

struct StreamState {
    camera_fb_t* fb;      // current frame being sent
    size_t index;         // how many bytes sent so far
    bool sending_header;  // are we sending the header or the jpeg?
    char header[64];      // the per-frame multipart header
    size_t header_len;
};

void handle_image(AsyncWebServerRequest* request) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        request->send(500, "text/plain", "Capture failed");
        return;
    }
    Serial.printf("Took a pic. Length: %d\n", fb->len);
    AsyncWebServerResponse* response = request->beginResponse(
        "image/jpeg",
        fb->len,
        [fb](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
            size_t remaining = fb->len - index;
            size_t toSend = min(remaining, maxLen);
            memcpy(buffer, fb->buf + index, toSend);
            if (index + toSend == fb->len) {
                esp_camera_fb_return(fb);
            }
            return toSend;
        }
    );
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Content-Disposition", "inline; filename=capture.jpg");
    response->addHeader("Content-Length", String(fb->len));
    response->setContentLength(fb->len);
    request->send(response);
}

void handle_stream(AsyncWebServerRequest* request) {
    // allocate state for this client connection
    StreamState* state = new StreamState();
    state->fb = NULL;
    state->index = 0;
    state->sending_header = false;
    state->header_len = 0;

    // begin a chunked multipart response
    AsyncWebServerResponse* response = request->beginChunkedResponse(
        "multipart/x-mixed-replace; boundary=frame",
        [state](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
            // if we have no current frame, capture one
            if (!state->fb) {
                state->fb = esp_camera_fb_get();
                if (!state->fb) {
                    // camera failed, send nothing this cycle
                    return 0;
                }

                // build the frame header
                state->header_len = snprintf(
                    state->header,
                    sizeof(state->header),
                    "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                    state->fb->len
                );
                state->index = 0;
                state->sending_header = true;
            }

            size_t written = 0;

            // send header bytes first
            if (state->sending_header) {
                size_t remaining = state->header_len - state->index;
                size_t toSend = min(remaining, maxLen);
                memcpy(buffer, state->header + state->index, toSend);
                state->index += toSend;
                written = toSend;

                if (state->index >= state->header_len) {
                    // header done, move to JPEG data
                    state->sending_header = false;
                    state->index = 0;
                }

                return written;
            }

            // send JPEG bytes
            size_t remaining = state->fb->len - state->index;
            size_t toSend = min(remaining, maxLen);
            memcpy(buffer, state->fb->buf + state->index, toSend);
            state->index += toSend;
            written = toSend;

            if (state->index >= state->fb->len) {
                // frame done, return buffer and reset for next frame
                esp_camera_fb_return(state->fb);
                state->fb = NULL;
                state->index = 0;

                // send the trailing boundary
                if (written + 2 <= maxLen) {
                    memcpy(buffer + written, "\r\n", 2);
                    written += 2;
                }
            }

            return written;
        }
    );

    response->addHeader("Access-Control-Allow-Origin", "*");

    // clean up state when client disconnects
    request->onDisconnect([state]() {
        if (state->fb) {
            esp_camera_fb_return(state->fb);
        }
        delete state;
        Serial.println("Stream client disconnected");
    });

    request->send(response);
    Serial.println("Stream client connected");
}

// void init_server() {
//     httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//     config.server_port = 80;
//     httpd_uri_t image_uri = {
//         .uri     = "/image",
//         .method  = HTTP_GET,
//         .handler = handle_image,
//         .user_ctx = NULL
//     };
//     httpd_uri_t stream_uri = {
//         .uri     = "/stream",
//         .method  = HTTP_GET,
//         .handler = handle_stream,
//         .user_ctx = NULL
//     };
//     if (httpd_start(&server, &config) == ESP_OK) {
//         httpd_register_uri_handler(server, &image_uri);
//         httpd_register_uri_handler(server, &stream_uri);
//         Serial.println("hosting http server successfully.");
//     } else {
//         Serial.println("unable to host http server.");
//     }
// }
// static esp_err_t handle_image(httpd_req_t *req) {
//     camera_fb_t *fb = esp_camera_fb_get();
//     if (!fb) {
//         Serial.println("Camera Capture failed!!");
//         httpd_resp_send_500(req);
//         return ESP_FAIL;
//     }
//     Serial.printf("someone requested a pic. Length: %d\n", fb->len);
//     httpd_resp_set_type(req, "image/jpeg");
//     httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
//     esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
//     esp_camera_fb_return(fb);
//     return res;
// }
// static esp_err_t handle_stream(httpd_req_t *req) {
//     Serial.println("Someone requested a video stream");
//     camera_fb_t* fb = NULL;
//     esp_err_t res = ESP_OK;
//     char part_header[128];
//     httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=123456789000000000000987654321");
//     httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
//     while (true) {
//         fb = esp_camera_fb_get();
//         if (!fb) {
//             Serial.println("Camera Capture failed!!");
//             res = ESP_FAIL;
//             break;
//         }
//         // send frame boundary
//         res = httpd_resp_send_chunk(req, "\r\n--123456789000000000000987654321\r\n", 36);
//         if (res != ESP_OK) {
//             esp_camera_fb_return(fb);
//             break;
//         }
//         size_t header_len = snprintf(
//             part_header,
//             sizeof(part_header),
//             "\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
//             fb->len);
//         // send header
//         res = httpd_resp_send_chunk(req, part_header, header_len);
//         if (res != ESP_OK) {
//             esp_camera_fb_return(fb);
//             break;
//         }
//         // send image binary
//         res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
//         esp_camera_fb_return(fb);
//         if (res != ESP_OK) {
//             break;
//         }
//     }
//     httpd_resp_send_chunk(req, NULL, 0); // signal end-of-stream
//     return res;
// }