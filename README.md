## Overview

This project explores inter-AS routing behaviour using ns-3.48.
It models a small multi-AS network and studies how topology,
routing metrics, and explicit forwarding policies affect path
selection, latency, and packet delivery.

## Experiments

### Baseline Routing
A four-AS linear topology:
AS1 → AS2 → AS3 → AS4

### Direct Peering
Adds a direct AS1–AS4 connection and evaluates path selection
and latency improvement.

### Policy Routing
Forces preference for the multi-hop AS1 → AS2 → AS3 → AS4
path using:
- static routing
- interface metrics

### Planned Experiments
- link failure and recovery
- dual-path rerouting
- convergence analysis

## Environment

- Ubuntu 24.04+
- ns-3.48
- C++23

## Running

The simulations are designed to run inside an ns-3.48
installation.

Example:

```
./ns3 run scratch/baseline
./ns3 run scratch/direct-peering
./ns3 run "scratch/policy-routing --mode=static"
./ns3 run "scratch/policy-routing --mode=metric"
```