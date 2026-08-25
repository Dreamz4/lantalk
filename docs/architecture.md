# LanTalk Architecture

## Components
- Network (TCP/UDP)
- Chat (Session, UI)
- Security (Crypto)
- Discovery (LAN)
- Config

## Threads
- Main UI thread
- Network event loop
- Discovery broadcast

## Data Flow
User -> UI -> Session -> Connection -> Network -> Peer

## Platform
Winsock2 / POSIX sockets.
