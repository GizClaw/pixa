# PIXA format

This document is the canonical format contract. PIXA v1 uses a 40-byte little-endian header with `PIXA` magic, clip and frame tables, and RGB565 key-frame payloads. The migrated runtime packages define the executable parser behavior while this contract is expanded with the table layouts during the migration.
