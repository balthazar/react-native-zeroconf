#!/usr/bin/env bash
# Builds the documentation website from docs/ and deploys it to the cluster.
# Usage: website/deploy.sh   (KUBE_CONTEXT defaults to dadonew)
set -euo pipefail

cd "$(dirname "$0")/.."
CONTEXT="${KUBE_CONTEXT:-dadonew}"
NAMESPACE=apps

python3 website/build.py docs website/dist/index.html

kubectl --context "$CONTEXT" -n "$NAMESPACE" create configmap zeroconf-docs \
  --from-file=index.html=website/dist/index.html \
  --from-file=nginx.conf=website/nginx.conf \
  --dry-run=client -o yaml | kubectl --context "$CONTEXT" apply -f -
kubectl --context "$CONTEXT" apply -f website/k8s.yaml

# The files are mounted with subPath, so pods only see new content after a restart
kubectl --context "$CONTEXT" -n "$NAMESPACE" rollout restart deployment/zeroconf-docs
kubectl --context "$CONTEXT" -n "$NAMESPACE" rollout status deployment/zeroconf-docs --timeout=120s
