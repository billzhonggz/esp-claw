# ESP-Claw Security Audit Report

**Project:** ESP-Claw (Espressif IoT AI Agent Framework)
**Path:** `C:\Users\Junru\source\esp32-p4-dev\esp-claw`
**Date:** 2026-04-29
**Scope:** All source files (C, Python, YAML, configuration)

---

## Critical Findings

### 1. No Authentication on HTTP API (CRITICAL)

**All HTTP endpoints** have **zero authentication**:

| Route | Method | Function | Purpose |
|---|---|---|---|
| `/api/config` | GET | `config_get_handler` | Reads ALL secrets (API keys, passwords, tokens) |
| `/api/config` | POST | `config_post_handler` | Writes all config fields |
| `/api/files` | GET | `files_list_handler` | Lists files in storage |
| `/api/files` | DELETE | `files_delete_handler` | Deletes files/directories |
| `/api/files/upload` | POST | `files_upload_handler` | Uploads files to storage |
| `/api/files/mkdir` | POST | `files_mkdir_handler` | Creates directories |
| `/files/*` | GET | `file_download_handler` | Downloads arbitrary files |
| `/api/wechat/login/start` | POST | `wechat_login_start_handler` | Starts WeChat OAuth |
| `/api/wechat/login/status` | GET | `wechat_login_status_handler` | Reads login status + token |
| `/api/wechat/login/cancel` | POST | `wechat_login_cancel_handler` | Cancels login flow |
| `/api/status` | GET | `status_handler` | System status |
| `/api/capabilities` | GET | `capabilities_get_handler` | Capability listing |
| `/api/lua-modules` | GET | `lua_modules_get_handler` | Lua module listing |
| `/api/webim/send` | POST | `webim_send_handler` | Sends IM messages as the device |
| `/ws/webim` | GET/WS | `webim_ws_handler` | Real-time messaging WebSocket |

**Exploit:** `curl http://<esp32-ip>/api/config` returns JSON containing all secrets.

**Files:** All handlers in `application/edge_agent/components/http_server/*.c`

---

### 2. Pipe-to-Shell in CI/CD Pipeline (CRITICAL)

**File:** `.gitlab/ci/build-doc.yml:14`
```yaml
- curl -fsSL https://d2lang.com/install.sh | sh -s --
```

Fetches and executes a shell script directly from the internet with **no checksum verification, no version pinning, no signature check, no TLS pinning**. If `d2lang.com` is compromised or a MITM attack succeeds, arbitrary code executes in the CI pipeline.

---

### 3. Lua Sandbox Completely Missing (CRITICAL)

**File:** `components/claw_capabilities/cap_lua/src/cap_lua_runtime.c:335`
```c
luaL_openlibs(L);
```

`luaL_openlibs()` loads **every** Lua standard library with no filtering:

| Function | What it enables |
|---|---|
| `os.execute()` | Run arbitrary system commands |
| `os.exit()` | Terminate the RTOS / crash the device |
| `os.remove()` | Delete any file |
| `io.open()` | Open, read, write ANY file |
| `io.popen()` | Run arbitrary commands and capture output |
| `load()` / `loadstring()` | Dynamically execute arbitrary Lua code |
| `dofile()` / `loadfile()` | Execute arbitrary Lua files |
| `debug.debug()` / `debug.getregistry()` | Inspect/modify runtime state |

**Worse:** `lua_run_script`, `lua_write_script`, `lua_run_script_async` are all marked `CLAW_CAP_FLAG_CALLABLE_BY_LLM` (`cap_lua.c:853-951`), meaning an LLM can be prompted to execute arbitrary Lua code.

**Exploit:** Prompt injection → LLM calls `lua_run_script` with:
```lua
os.execute("poweroff")
io.popen("cat /nvs/credentials.bin"):read("*a")
loadstring("os.execute('reboot')")()
```

---

## High Severity Findings

### 4. Use-After-Free in Capability Realloc (HIGH)

**File:** `components/claw_modules/claw_cap/src/claw_cap.c:729-742`

Two instances of an unsafe `realloc` rollback pattern. When `realloc` of the snapshot array fails after `realloc` of the slots array succeeds:
```c
new_slots = realloc(original_slots, new_capacity * sizeof(*new_slots));
if (!new_slots) { return ESP_ERR_NO_MEM; }
new_snapshot = realloc(original_snapshot, new_capacity * sizeof(*new_snapshot));
if (!new_snapshot) {
    // NEW_SLOTS IS VALID BUT S_RUNTIME.DESCRIPTOR_SLOTS STILL POINTS TO FREED MEMORY
    ...
    return ESP_ERR_NO_MEM;  // s_runtime.descriptor_slots is a DANGLING POINTER!
}
```

Also at lines 790-801 (group expansion), identical pattern.

---

### 5. MCP Server: No Authentication (HIGH)

**File:** `components/claw_capabilities/cap_mcp_server/src/cap_mcp_server.c:381`
```c
err = esp_mcp_mgr_register_endpoint(s_mgr, s_config.endpoint, NULL);
```

Registered with `NULL` auth handler. Any LAN device can:
- Connect to the MCP server (default port 18791, endpoint `/mcp_server`)
- Invoke `router.emit_event` to inject arbitrary events into the system event bus

---

### 6. MCP Client: Server-Side Request Forgery (HIGH)

**File:** `components/claw_capabilities/cap_mcp_client/src/cap_mcp_client_core.c:74-111`

The MCP client accepts a `server_url` from user/LLM input and makes HTTP POST requests with **no validation against private/reserved IP ranges**:
- `localhost` / `127.0.0.1`
- `169.254.x.x` (cloud metadata)
- `10.x.x.x`, `172.16-31.x.x`, `192.168.x.x`

Both `mcp_list_tools` and `mcp_call_tool` callable by LLM.

---

### 7. PyPI Packages Not Version-Pinned (HIGH)

**File:** `.gitlab/ci/build.yml:48`
```yaml
- pip install idf_build_apps esp-bmgr-assist
```

Installs latest versions with no version pinning. Dependency confusion/hijacking risk.

---

### 8. No Manual Approval Gate for Deployments (HIGH)

**File:** `.gitlab/ci/deploy.yml:13-14`

Any push to `master` automatically deploys to Cloudflare Pages production with **no human approval**. No `when: manual` gate.

---

### 9. Storage Lua Module: No Path Validation (HIGH)

**File:** `components/lua_modules/lua_module_storage/src/lua_module_storage.c:110-290`

All functions (`read_file`, `write_file`, `remove`, `rename`, `stat`, `listdir`) accept **arbitrary absolute paths** with no restriction to the storage base directory. `..` is not blocked in `join_path`.

```lua
local s = require("storage")
s.read_file("/data/credentials.json")  -- reads arbitrary files
s.remove("/boot/loader.bin")           -- deletes system files
```

---

### 10. Unprotected CI/CD Variables (HIGH)

**File:** `.gitlab/ci/deploy.yml:26,35`

Cloudflare API token and other secrets referenced as `$VARIABLE_NAME` but **not declared as masked/protected** in YAML. Risk of exposure in CI logs.

---

## Medium Severity Findings

| # | Finding | File | Details |
|---|---------|------|---------|
| 11 | Sensitive data via plain HTTP | `http_server_config_api.c:161-230` | All secrets returned in `/api/config` GET over HTTP |
| 12 | Unauthenticated file upload/delete | `http_server_files_api.c:125-211` | No auth on file upload, delete, mkdir operations |
| 13 | No CSRF protection | All POST/DELETE handlers | No anti-CSRF tokens, no Origin/Referer checks |
| 14 | No TLS/HTTPS on config server | `http_server_core.c:62-67` | Plaintext transport for all secrets over open Wi-Fi |
| 15 | WeChat token leaked via API | `http_server_wechat_api.c:86-87` | Bearer token returned in unauthenticated API response |
| 16 | Plaintext credentials in RAM | All IM modules | Bot tokens, app secrets, API keys in plaintext global structs |
| 17 | File exfiltration via LLM | IM `send_image`/`send_file` functions | No path validation on files read and sent to IM platforms |
| 18 | QQ attachment SSRF | `cap_im_qq.c:798-843` | Attachment URLs parsed from messages downloaded without validation |
| 19 | MCP server event injection | `cap_mcp_server.c:179-222` | `router.emit_event` allows arbitrary event injection |
| 20 | `strcat` without bounds enforcement | `app_claw_cli.c`, `basic_demo_cli.c`, `cmd_cap_scheduler.c`, `cmd_cap_router_mgr.c` | Brittle pattern: pre-calculated size may drift from actual during maintenance |
| 21 | Unchecked `strdup` return | `http_server_config_api.c:176,179` | NULL → filter is silently skipped → all config fields emitted |
| 22 | Integer overflow in display alloc | `display_hal.c:921,1259,1317,1586` | `w * h * 2` could overflow 32-bit `size_t` |
| 23 | NVS encryption not enabled | `sdkconfig.defaults` | All secrets stored as plaintext on flash via `nvs_set_str` |
| 24 | Open SoftAP (no password) | `wifi_manager.c:120` | `WIFI_AUTH_OPEN` — anyone within radio range can connect |
| 25 | Config API transmits secrets over plain HTTP | `http_server_config_api.c` | Credentials in clear over open Wi-Fi during provisioning |
| 26 | Docker images not pinned by digest | Multiple CI YAML files | `node:24-alpine`, `espressif/idf:release-v5.5` — mutable tags |
| 27 | pnpm without frozen-lockfile | `build-doc.yml:17` | Dependency drift between pipeline runs |
| 28 | Build artifacts broadly accessible | `build.yml:16-37` | No `access: members` restriction on firmware binaries |
| 29 | `capability.call()` impersonates system caller | `lua_module_capability.c:214` | `CLAW_CAP_CALLER_SYSTEM` used, bypassing caller-identity checks |
| 30 | Feishu app secret sent redundantly | `cap_im_feishu.c:570-577` | `app_secret` sent twice in same request |

---

## Low Severity Findings

| # | Finding | File |
|---|---------|------|
| 31 | Missing security headers (CSP, XFO, HSTS) | All response handlers |
| 32 | No rate limiting / DoS protection | All endpoints |
| 33 | Captive portal redirect trusts runtime IP | `http_server_core.c:33` |
| 34 | Config field type validation missing | `http_server_config_api.c:251-258` |
| 35 | No recursion depth limit in Lua | `cap_lua_runtime.c:344` |
| 36 | Telegram bot token in URL logs | `cap_im_tg.c:181,195,930,943` |
| 37 | Log injection in error messages | Multiple IM modules |
| 38 | Scratch buffer info leak | `http_server_files_api.c:102-112` |
| 39 | Verbose build logging (`-vv`) | `build.yml:54` |
| 40 | Path truncation in webim URL builder | `http_server_webim_api.c:194` |
| 41 | WeChat QR token pushed to main state | `cap_im_wechat.c:2533-2538` |
| 42 | MCP server port from console command | `cmd_cap_mcp_server.c:98-105` |
| 43 | QQ infinite retry on token expiry | `cap_im_qq.c:1095,1162-1165` |

---

## Positive Security Practices

| Practice | Status | Details |
|----------|--------|---------|
| Hardcoded credentials | **CLEAN** | No hardcoded API keys, tokens, or passwords in source |
| `sprintf`/`strcpy` usage | **CLEAN** | `snprintf`/`strlcpy` used consistently; zero instances of `sprintf`/`strcpy` |
| `malloc` NULL checks | **CLEAN** | ~214 `malloc`/`calloc` calls — all checked (except 2 `strdup` calls) |
| Format string vulnerabilities | **CLEAN** | All `printf` calls use constant format strings |
| `system()`/`popen()` | **CLEAN** | Not used outside Lua (which is the sandbox issue) |
| `.gitignore` coverage | **CLEAN** | `sdkconfig`, `sdkconfig.priv`, `.env` properly excluded |
| Branch protection (CI) | **CLEAN** | Non-protected branch pipelines disabled (forces MR workflow) |
| Pre-commit hooks | **CLEAN** | Copyright, formatting, commit message conventions enforced |
| MQTT usage | **CLEAN** | No MQTT/TCP server with cleartext credentials |
| Double free | **CLEAN** | All `free` calls trace to single-ownership patterns |
| Uninitialized variables | **CLEAN** | All locals initialized or written before read |
| User input in format strings | **CLEAN** | Not found in any code path |

---

## Priority Remediation Actions

### Immediate

1. **Add authentication** to all HTTP API endpoints (session-based or bearer token)
2. **Implement Lua sandbox** — remove `os`, `io`, `load`, `loadstring`, `dofile`, `loadfile`, `debug`, `require` from globals after `luaL_openlibs()`
3. **Fix use-after-free** in `claw_cap.c` — update `s_runtime.descriptor_slots`/`group_slots` immediately after first `realloc` succeeds
4. **Replace pipe-to-shell** in `build-doc.yml:14` with checksum-verified installation or pre-built Docker image
5. **Add SSRF protection** to MCP client — reject requests to private/reserved IP ranges
6. **Add MCP server authentication** — non-NULL auth callback for endpoint registration

### High Priority

7. **Pin all CI dependencies** — Docker digests, PyPI versions, npm packages
8. **Add manual approval gate** for production deployments in `deploy.yml`
9. **Add path validation** to storage Lua module against `s_storage_base_path`
10. **Restrict file paths** in LLM-callable IM send functions to a sandboxed directory
11. **Restrict build artifact access** to project members (`access: members`)
12. **Remove `CLAW_CAP_FLAG_CALLABLE_BY_LLM`** from `lua_write_script` and `lua_run_script`

### Medium Priority

13. **Enable NVS encryption** in `sdkconfig.defaults`
14. **Add WPA2 password** to provisioning SoftAP
15. **Add TLS to HTTP config server** (self-signed certificate)
16. **Replace `strcat`** with `memcpy` + offset tracking in all CLI join functions
17. **Add NULL checks** after `strdup` in `http_server_config_api.c`
18. **Add overflow-safe multiplication** helpers in `display_hal.c`
19. **Add security headers** (CSP, X-Frame-Options, X-Content-Type-Options) to all responses
20. **Add rate limiting** to all HTTP endpoints
21. **Change `caller`** in `capability.call()` from `CLAW_CAP_CALLER_SYSTEM` to `CLAW_CAP_CALLER_LUA`
22. **Add URL validation** before attachment downloads in QQ, TG, WeChat modules

---

## Vulnerability Count

| Severity | Count |
|----------|-------|
| CRITICAL | 3 |
| HIGH | 7 |
| MEDIUM | 20 |
| LOW | 13 |
| **Total** | **43** |
