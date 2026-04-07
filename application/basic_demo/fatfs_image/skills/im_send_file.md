# IM File Return

Use this skill when the user wants the device to send an existing local non-image file back to a chat.

## When to use
- The user asks to send back a file, report, log, archive, JSON, CSV, or other non-image output.
- The target conversation is already the active Telegram, QQ, Feishu, or WeChat chat, or the user provides an explicit target `chat_id`.

## Available tools
- `list_dir`: inspect device storage and confirm the file path
- `read_file`: inspect small text files before sending when needed
- `cap_cli`: run `cap call <cap_name> '<json>'` to invoke the transport-specific send capability indirectly

## WeChat status
- This firmware variant exposes WeChat text and image send, but does not expose `wechat_send_file`.
- If the user asks to send a generic non-image file to WeChat, explain that the current WeChat path only supports text and image send.
- If the requested asset is actually an image, switch to the `im_send_picture` skill and use `wechat_send_image` through `cap_cli`.

## Path guidance
- Prefer real local paths that already exist on the device.
- Common roots in this demo are `/spiffs`, `/spiffs/lua`, `/spiffs/qq`, `/spiffs/tg`, or application-managed subdirectories.
- Use `list_dir` first if the exact path is unknown.
- Use `read_file` only for small text inspection, not for binary payloads.

## Sending rules
- Use this skill only for non-image files such as `.txt`, `.json`, `.log`, `.csv`, `.zip`, or archives.
- Use `cap_cli` with `cap call tg_send_file '<json>'`, `cap call qq_send_file '<json>'`, or `cap call feishu_send_file '<json>'`.
- The JSON payload should include an explicit `chat_id`, `path`, and optional `caption`.
- Use Telegram file capability for Telegram chats, QQ file capability for QQ chats, and Feishu file capability for Feishu chats.
- Do not claim WeChat generic file-send support unless a real `wechat_send_file` capability exists.
- Pass `caption` only when the user wants an accompanying message.
- The second argument of `cap call` must be one complete JSON string. Do not rewrite it as CLI flags or `key=value`.

## Examples

Send a file to a Telegram chat through `cap_cli`:
```text
cap call tg_send_file '{"chat_id":"123456789","path":"/spiffs/reports/status.json","caption":"Latest status report."}'
```

Send a file to a QQ group through `cap_cli`:
```text
cap call qq_send_file '{"chat_id":"group1234567890","path":"/spiffs/reports/status.json","caption":"Latest status report."}'
```

Send a file to a Feishu chat through `cap_cli`:
```text
cap call feishu_send_file '{"chat_id":"ou_xxx","path":"/fatfs/data/reports/status.json","caption":"Latest status report."}'
```

## Workflow
1. Confirm the target file exists with `list_dir` if needed.
2. Identify the target channel: Telegram, QQ, Feishu, or WeChat.
3. Choose `tg_send_file`, `qq_send_file`, or `feishu_send_file` based on the target channel.
4. For WeChat, stop and explain that this firmware variant does not expose `wechat_send_file`.
5. Execute the capability through `cap_cli` as `cap call <cap_name> '<json>'` with explicit `chat_id`, `path`, and optional `caption`.
6. Tell the user whether the send succeeded.

## Notes
- This skill only sends files that already exist on the device filesystem.
- Do not use this skill for `.jpg`, `.jpeg`, `.png`, `.gif`, or `.webp`; use the im_send_picture skill instead.
- If the user wants WeChat delivery and the asset is an image, use `wechat_send_image` through the picture skill instead.
