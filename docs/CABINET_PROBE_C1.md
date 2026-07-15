# HANSO Cabinet Probe C1

Cabinet Probe C1 is the deterministic excitation signal used by direct Cabinet
position captures. It is separate from HANSO Probe A1 because a linear cabinet
IR needs deconvolution and decay capture, not nonlinear model-fit segments.

## Signal layout

At 48 kHz the probe is exactly 8.0 seconds:

| Time | Signal | Purpose |
| --- | --- | --- |
| 0.0-6.0 s | synchronized exponential sine sweep, 35 Hz-12 kHz | transfer-function excitation |
| 6.0-8.0 s | silence | speaker, cabinet, and room decay tail |

The sweep peak never exceeds the calibrated capture amplitude. The synchronized
end condition and short raised-cosine fades reduce boundary energy.

## IR extraction

After latency alignment, HANSO Lab computes a regularized frequency-domain
inverse:

```text
H[k] = Y[k] * conj(X[k]) / (|X[k]|^2 + max(|X|^2) * 1e-7)
```

The inverse FFT is mono-summed, searched for the direct response within the
first 250 ms, cropped with up to 1 ms pre-roll, and retained for 50-1000 ms.
The tail threshold is -70 dB relative to the direct peak, followed by a 20 ms
fade. The final IR is normalized to -1 dBFS and stored as PCM16 with role
`cabinet-ir`.

Extraction failure is a capture error. A raw aligned sweep response must never
be marked as a captured IR.

## Mic placement contract

- Reference distance: 2 cm from grille cloth to microphone capsule center.
- Cone, Cone Edge, and Edge: microphone axis perpendicular to the grille.
- Off-Axis: retain the same capsule point and 2 cm distance, rotate 30 degrees.
- Keep microphone preamp gain unchanged between all position slots.
