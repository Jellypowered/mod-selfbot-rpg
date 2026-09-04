# JLYRPG2 addon protocol v1

Transport: WoW `LANG_ADDON` messages with the project-unique prefix `JLYRPG2`.
Requests may use PARTY/RAID while grouped, but all server replies are directed
back to the requesting selfbot via WHISPER to prevent catalog/source/status
cross-talk. Fields are tab separated. Normal chat is never interpreted as protocol traffic.

Every client request has a decimal request ID. The server consumes every frame
claiming `JLYRPG2`, including malformed ones, so it cannot fall through into a
legacy playerbot handler.

## Client → server

```text
JLYRPG2\t1\tHELLO\t<requestId>
JLYRPG2\t1\tMATERIAL_CATALOG\t<requestId>
JLYRPG2\t1\tMATERIAL_SOURCES\t<requestId>\t<material>
JLYRPG2\t1\tSTART_MATERIAL\t<requestId>\tmaterial\t<material>\t<durationMinutes>\t<quantity>
JLYRPG2\t1\tSTART\t<requestId>\t<mining|herbalism|both|zone>\t<resource>\t<durationMinutes>
JLYRPG2\t1\tSTOP\t<requestId>
JLYRPG2\t1\tSTATUS\t<requestId>
JLYRPG2\t1\tSET\t<requestId>\t<key>\t<value>
```

## Server → client

```text
JLYRPG2\t1\tHELLO_ACK\t<requestId>\t1
JLYRPG2\t1\tCAPABILITIES\t<requestId>\t1\t<comma-separated-capabilities>
JLYRPG2\t1\tMATERIAL_CATALOG\t<requestId>\t<index>\t<total>\t<itemId>\t<key>\t<displayName>\t<family>\t<methods>
JLYRPG2\t1\tMATERIAL_SOURCE\t<requestId>\t<index>\t<total>\t<creatureEntry>\t<method>\t<chance>\t<normal|quest>
JLYRPG2\t1\tMATERIAL_STATUS\t<requestId>\t<active>\t<itemId>\t<gathered>\t<goal>\t<kills>\t<remainingSec>\t<skinning>
JLYRPG2\t1\tACK\t<requestId>\t<opcode>
JLYRPG2\t1\tERROR\t<requestId>\t<text>
JLYRPG2\t1\tSTATUS\t<active>\t<phase>\t<reason>\t<profession>\t<routeNodes>\t<gathers>\t<items>\t<itemsPerMin>\t<itemsPerSec>\t<targetSpawn>\t<durationSec>\t<remainingSec>
JLYRPG2\t1\tSETTING\t<key>\t<value>
```

`durationMinutes` is decimal `0` for unlimited or `1..10080`. The addon enables
controls only after `HELLO_ACK`, preserves multi-word resource names as one
`START` field, and correlates acknowledgements by request
ID. `/sbrpgchat <command>` is an intentionally manual-only emergency
chat fallback; it is never automatically selected by the addon.
