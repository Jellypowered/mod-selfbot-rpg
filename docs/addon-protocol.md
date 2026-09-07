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
JLYRPG2\t1\tSTART_FISHING\t<requestId>\t<fish|zone>\t<target>\t<durationMinutes>\t<quantity>\t<prioritizePools>\t<openWaterOnly>
JLYRPG2\t1\tSTART\t<requestId>\t<mining|herbalism|both|zone>\t<resource>\t<durationMinutes>
JLYRPG2\t1\tSTOP\t<requestId>
JLYRPG2\t1\tSTATUS\t<requestId>
JLYRPG2\t1\tSET\t<requestId>\t<key>\t<value>
JLYRPG2\t1\tSET_CONFIG\t<requestId>\t<key>\t<value>
```

## Server → client

```text
JLYRPG2\t1\tHELLO_ACK\t<requestId>\t1
JLYRPG2\t1\tCAPABILITIES\t<requestId>\t1\t<comma-separated-capabilities>
JLYRPG2\t1\tMATERIAL_CATALOG\t<requestId>\t<index>\t<total>\t<itemId>\t<key>\t<displayName>\t<family>\t<methods>
JLYRPG2\t1\tMATERIAL_SOURCES_END\t<requestId>\t<total>
JLYRPG2\t1\tMATERIAL_SOURCE\t<requestId>\t<index>\t<total>\t<creatureEntry>\t<method>\t<chance>\t<normal|quest>
JLYRPG2\t1\tMATERIAL_STATUS\t<requestId>\t<active>\t<itemId>\t<gathered>\t<goal>\t<kills>\t<remainingSec>\t<harvest>\t[<phase>]
JLYRPG2\t1\tACK\t<requestId>\t<opcode>\t[<key>]
JLYRPG2\t1\tERROR\t<requestId>\t<text>
JLYRPG2\t1\tSTATUS\t<active>\t<phase>\t<reason>\t<profession>\t<routeNodes>\t<gathers>\t<items>\t<itemsPerMin>\t<itemsPerSec>\t<targetSpawn>\t<durationSec>\t<remainingSec>
JLYRPG2\t1\tSETTING\t<key>\t<value>
```

`durationMinutes` is decimal `0` for unlimited or `1..10080`. `MATERIAL_STATUS` may include a trailing phase field; older clients safely ignore it. The addon settings window exposes the current `mod-selfbot-rpg.conf.dist` values through `SET_CONFIG`; values are validated and applied to the running module session, while the addon persists them in `SelfBotRPGDB.Settings`. `UseLures` is an optional best-effort fishing setting. `START_FISHING` supports selected-fish and current-zone modes; open-water casting is the default, while pool prioritization is opt-in and `openWaterOnly` disables pool routing. Start controls on the advertised `CAPABILITIES` set, preserves multi-word
resource names as one `START` field, and correlates catalog/source chunks by
request ID and sequence/total fields. `MATERIAL_SOURCES_END` is sent even when
the source set is empty, so the client can distinguish an empty result from an
incomplete response. The addon stores panel position, selected profession,
resource, material, search text, duration, and quantity in `SelfBotRPGDB`.
`/sbrpgchat <command>` is an intentionally manual-only emergency chat fallback;
it is never automatically selected by the addon.

Node `STATUS` phase/reason fields may report live-node reroutes, confirmed-empty
route-point skips, post-combat loot recovery, or bounded gather-pending timeout.
These are informational only; node interaction and loot remain owned by stock
playerbot actions. Mount-aware travel is server-side and does not add a protocol
field: the server may mount before node, live-node, hotspot, water, pool, or
return movement, then dismount for interaction. Future danger screening should
remain status-reason text or a versioned capability, not an unannounced change
to the positional STATUS parser. The optional `mod-junk-to-gold` module does not change this
protocol and is not required for the addon or server module to function.

## Source ownership

Request handlers/configuration and response publishers live in `src/Protocol/`; script registration and commands live in `src/Integration/`. This extraction does not revise wire fields, capabilities, commands or addon saved variables. See [architecture](architecture.md).
