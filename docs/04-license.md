# 04. license, device id, updates

## what the original does

strings that describe the licensing client:

| string | meaning |
| --- | --- |
| dotavoicelab-licensing | name of the licensing library |
| dotavoicelab-desktop | product id |
| me.razmut.desktop | audience of the token |
| https://razmut.me/api/v1/ | api prefix |
| licensing/lease.json | local lease |
| licensing/installation-id | install uuid |
| .licensing/device.dat | encrypted device blob |
| RazmutDotaVoiceLab.Subscription.v1 | state file name |

claims, from the serde schema and the jwt strings:

```
iss, aud, sub, deviceId, iat, exp, subscriptionExpiresAt, kty, ne
```

plus `RS256`, `alg`, jwk-ish `PublicKey` with 3 fields, and crates `rsa 0.9.10`,
`num-bigint-dig`, `const-oid`, `base64 0.22.1`. so the lease is a signed jwt
verified on the client with an embedded public key. `device.dat` is protected with
`CryptProtectData` (dpapi), so copying the folder to another machine does not move
the license.

checks the client performs, again visible as error strings:

```
Lease expired
Invalid lease lifetime
Invalid lease identity
Lease identity mismatch
Invalid lease header
Invalid lease encoding
Invalid lease length
Untrusted licensing origin
Invalid license audience
Invalid public key
```

updates go through `tauri-plugin-updater` 2.14.1 against
`https://razmut.me/updates-tauri/latest.json`, signed with minisign. the public
key is embedded (inside a base64 blob starting with `dW50cnVzdGVkIGNvbW1lbnQ6`,
key id 17794B3D92978FCD), and the plugin config carries `pubkey`,
`require-signed-version` and `allowDowngrades`.

## what this rewrite does

same shape, no network:

- `licensing/installation-id`: random uuid, written once
- `licensing/device.dat`: hex of the device id xored with a rolling key. this is a
  stand in for dpapi, not a security feature
- `licensing/lease.json`: same field names (iss, aud, sub, deviceId, iat, exp),
  no signature
- `import-complete`: marker file, same name as the original
- `state.ini`: module state and a cached license summary

device id is the first 8 hex of fnv1a(uuid + "|" + computer name), so a copied
folder on another machine gives a different id.

the key format is my own: `RZM-XXXX-XXXX-XXXX-XXXX`, crockford base32 body of 12
characters plus a crc8 in the last group. `keygen` prints valid ones,
`activate` checks them offline. there is no server, no renewal, no signature
check.

## differences that matter

| topic | original | here |
| --- | --- | --- |
| lease | rs256 jwt from razmut.me | plain json, no signature |
| device blob | dpapi | xor hex |
| renewal | yes, `/api/v1/` | no |
| clock checks | expiry, identity, origin | expiry only |
| updater | minisign against a live endpoint | not implemented |

if you ever need to look at the real protocol, that is a separate job: mitmproxy,
your own account, a machine you do not care about.
