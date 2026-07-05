# UE5 nDisplay ← splatcast NDI Setup (Phase 5, documentation-only task)

How to consume the splatcast NDI stream as the nDisplay inner-frustum texture. This repo
ships no UE code — these are operator steps for the UE machine.

## Prerequisites (UE machine)

- UE 5.3+ project with your nDisplay config (the stage project).
- **NDI SDK for Unreal Engine** (a.k.a. "NDI IO Plugin", free from <https://ndi.video/tools>):
  install the plugin into the engine or project, enable `NDI IO Plugin` in
  Edit → Plugins, restart the editor.
- splatcast machine and UE machine on the same network (NDI discovery is mDNS; same
  subnet, or configure NDI Access Manager/Discovery Server if VLANs separate them).

## Receive the stream

1. Start splatcast with NDI on the render machine:
   `splatcast.exe scene.ply --ndi splatcast --fps 25 --freed-port 8001`
   (source appears on the network as `<HOSTNAME> (splatcast)`).
2. In UE: Content Browser → Add → **NDI → NDI Media Receiver** (the plugin's receiver
   asset). Set its **Source Name** to the splatcast source (picker lists discovered
   sources). Set bandwidth Highest; disable audio.
3. The receiver asset exposes a **Media Texture** (create one from the receiver if the
   plugin version doesn't auto-create: right-click receiver → Create Media Texture).
4. Sanity check in-editor: drop the media texture on a plane in the level — you should
   see the splat render live.

## Feed the nDisplay inner frustum

1. Open the **nDisplay Config** asset → select the **ICVFX Camera** component.
2. Details → **In-Camera VFX → Media** section (UE 5.3+: "Media" on the ICVFX camera):
   enable **Media Input**, and set the **Media Source** to the NDI receiver's media
   source / texture (plugin version dependent: either assign the Media Texture in a
   Media Input Group, or use a Media Plate referencing the receiver).
3. If the stage uses **media sharing** across cluster nodes, add the media input to the
   ICVFX camera's *Media Input Groups* with the node list — one node receives NDI and
   shares to the others — instead of every node pulling the NDI stream separately.
4. **Disable UE's own inner-frustum rendering** for this camera (the media input
   replaces it): the media configuration on the ICVFX camera does this when media input
   is active.
5. Overscan: match splatcast's `--overscan` percentage in the ICVFX camera's overscan
   settings so the wall crop uses our padding (Mode B contract; 0% for Mode A).

## Timing expectations (plan §6.6)

NDI is free-running — frames arrive when they arrive, and UE resamples onto its own
genlocked cadence. Expect 1–3 frames of receiver-side buffering and occasional cadence
beat against genlock. **Do not chase sync artifacts in this phase** — note them; the
ST 2110/PTP path (Phase 6) is the fix. Latency is measured with the flash-frame
procedure in `docs/verification/phase-5.md` §5.
