# JLYRPG addon protocol v1

Transport: WoW `LANG_ADDON` messages with the project-unique prefix `JLYRPG`.
Use PARTY/RAID while grouped, and WHISPER to self while solo. Fields are tab
separated. Never interpret normal chat as protocol traffic.

## Client → server

```text
JLYRPG\t1\tSTART\t<mining|herbalism|both|zone>\t<resource>
JLYRPG\t1\tSTOP
JLYRPG\t1\tSTATUS
JLYRPG\t1\tSET\t<key>\t<value>
```

## Server → client

```text
JLYRPG\t1\tSTATUS\t<active>\t<mode>\t<routeNodes>\t<gathers>\t<items>\t<itemsPerMin>\t<itemsPerSec>\t<targetSpawn>
JLYRPG\t1\tSETTING\t<key>\t<value>
JLYRPG\t1\tDEBUG\t<text>
JLYRPG\t1\tERROR\t<text>
```

The addon accepts only prefix `JLYRPG` and protocol version `1`. The panel is
the source of truth: it requests status on open and at most once every two
seconds while visible. It does not parse system or whisper chat for status.
