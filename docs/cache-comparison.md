# Cache Comparison

| Feature | Memcached | Redis / Valkey | Dragonfly |
| --- | --- | --- | --- |
| Primary Role | Pure volatile cache | In-memory data store / cache | In-memory data store / cache |
| Snapshots (RDB) | ❌ None | ✅ Periodic point-in-time disk dumps | ✅ Periodic point-in-time disk dumps |
| Write-Ahead Log (AOF) | ❌ None | ✅ Append-Only File (every write/sec) | ❌ Snapshot-focused |
| Behavior on Restart | All data wiped (cold start) | Reloads state from disk/AOF | Reloads state from snapshot |
| Intended Use Case | Ephemeral, easily reproducible state | State that requires recovery after restart | High-scale state caching |
