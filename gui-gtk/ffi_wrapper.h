/* Thin C header exposing only the extern "C" symbols from libopenlawsvpn_ffi.cpp.
 * Used by bindgen — avoids pulling in C++ headers. */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*LogCallback)(const char* message, void* user_data);

typedef struct {
    char* saml_url;
    char* state_id;
    char* remote_ip;
} Phase1ResultC;

void* openvpn_client_new(const char* config_path);
void  openvpn_client_free(void* client);
void  openvpn_client_set_connect_mode(void* client, int mode);
void  openvpn_client_set_log_level(void* client, int level);
void  openvpn_client_set_log_callback(void* client, LogCallback callback, void* user_data);
Phase1ResultC openvpn_client_connect_phase1(void* client);
void  openvpn_client_connect_phase2(void* client, const char* state_id, const char* token, const char* remote_ip);
void  openvpn_client_disconnect(void* client);
void  openvpn_free_string(char* s);

#ifdef __cplusplus
}
#endif
