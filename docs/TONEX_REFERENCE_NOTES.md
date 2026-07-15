# TONEX workflow reference notes

These notes record product/workflow lessons adopted for HANSO Lab. The supplied
YouTube URL could not be opened in the available browser because of a URL policy;
the conclusions below were checked against IK Multimedia's official TONEX and
TONEX Capture pages instead.

## Adopted now

- Keep connection setup, input/output level calibration, capture, processing,
  comparison, metadata, and save/finalize as explicit workflow phases.
- Keep calibration as a hard gate before capture anchors.
- Insert a finished capture into the matching Tone Preview slot and preserve
  real-return versus model A/B comparison.
- Treat capture-authored pedals as static nonlinear systems: distortion,
  overdrive, fuzz, boost, and fixed EQ. Modulation, delay, reverb, and moving
  parameters remain separate runtime effects and are not represented as a
  captured compact model.
- Record deterministic probe identity and per-anchor duration in package
  metadata so capture and model processing can be separated safely.

## Later work

- Batch capture: collect multiple rigs first and process/train them later or on
  another machine.
- Portable capture jobs: preserve the dry probe, return WAV, routing, level,
  and target metadata as one resumable job.
- Multi-return capture: support two microphones or mic+DI with explicit phase
  alignment and independent source metadata.

## Official references

- https://www.ikmultimedia.com/products/tonex/?p=modeler
- https://www.ikmultimedia.com/products/tonex/?pkey=tonex-sw-cs
- https://www.ikmultimedia.com/products/tonexcapture/index.php?p=info
- https://www.ikmultimedia.com/news/?id=TONEX1.7.4Release
