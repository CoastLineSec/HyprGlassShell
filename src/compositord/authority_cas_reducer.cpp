#include "authority_cas_reducer.h"

#include "identity.h"

namespace HyprShelld::Compositor {
namespace {

[[nodiscard]] bool
currentTupleHasCanonicalGrammar(const CurrentAuthorityCasV2View &current) {
  return isCanonicalIdentifier(current.authorityId) &&
         isCanonicalSha256Digest(current.catalogDigest) &&
         isCanonicalSha256Digest(current.actionCatalogDigest);
}

} // namespace

AuthorityCasDecision
reduceAuthorityCasV2(const CurrentAuthorityCasV2View &current,
                     const ExpectedAuthorityCasV2View &expected) {
  switch (current.access) {
  case AuthorityAccess::Unavailable:
    return AuthorityCasDecision::Unavailable;
  case AuthorityAccess::ReadOnly:
  case AuthorityAccess::Writable:
    break;
  default:
    return AuthorityCasDecision::Unavailable;
  }

  if (!currentTupleHasCanonicalGrammar(current)) {
    return AuthorityCasDecision::Unavailable;
  }
  if (current.access == AuthorityAccess::ReadOnly) {
    return AuthorityCasDecision::ReadOnly;
  }
  if (!isCanonicalIdentifier(expected.authorityId) ||
      expected.authorityId != current.authorityId) {
    return AuthorityCasDecision::StaleAuthority;
  }
  if (!isCanonicalSha256Digest(expected.catalogDigest) ||
      !isCanonicalSha256Digest(expected.actionCatalogDigest) ||
      expected.catalogDigest != current.catalogDigest ||
      expected.actionCatalogDigest != current.actionCatalogDigest) {
    return AuthorityCasDecision::StaleCatalogDigest;
  }
  if (expected.revision != current.revision) {
    return AuthorityCasDecision::StaleRevision;
  }
  return AuthorityCasDecision::Proceed;
}

} // namespace HyprShelld::Compositor
