# SBRPG addon protocol v1

Transport: WoW `LANG_ADDON` messages with prefix `SBRPG`, sent via WHISPER
channel directed at the player themselves. Never broadcast to PARTY/RAID.
Fields use tab delimiters.

## Client → server

```
SBRPG	1	START	<mining|herbalism|both|zone>	<resource>
SBRPG	1	STOP
SBRPG	1	STATUS
SBRPG	1	SET	<key>	<value>
```

For Zone mining/herbalism, `START` sends `zone` as mode and the profession as
resource. For Both Zone, it sends `both`, `zone`.

## Server → client

```
SBRPG	1	STATUS	<active>	<mode>	<routeNodes>	<gathers>	<items>	<itemsPerMin>	<itemsPerSec>	<targetSpawn>
SBRPG	1	SETTING	<key>	<value>
SBRPG	1	DEBUG	<text>
SBRPG	1	ERROR	<text>
```

The server is authoritative. The addon renders these frames and does not infer
run state from system chat. The panel requests `STATUS` on open and no more
than once every two seconds while visible. Messages are self-whispered only;
never send SBRPG frames to PARTY or RAID. Malformed/unknown frames are ignored.
