# LanTalk Protocol

Wire format: Magic | Version | Type | Length | Flags | Payload
JSON schemas for messages.
Lifecycle: HELLO -> AUTH_REQUEST -> AUTH_RESPONSE -> AUTH_OK -> chat
Framing: 4-byte lengths.
Byte order: Network (Big-Endian).
Discovery: UDP Broadcast on port 31337
Versioning: 1.0.0
Security: TLS with PSK (optional).
