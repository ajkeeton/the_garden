# Mycelial
Central state annd command hub for the garden

## Usage

```
python src/main.py
```

The application listens on port 7777 and also advertises itself with mDNS.

## Protocol

Each message in the protocol follows this structure:

- **Type**: 2 bytes
- **Version**: 2 bytes
- **Length**: 2 bytes
- **Payload**: Arbitrary payload
