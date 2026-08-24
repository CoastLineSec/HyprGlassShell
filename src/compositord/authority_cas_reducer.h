#pragma once

#include <QStringView>
#include <QtTypes>

namespace HyprShelld::Compositor {

enum class AuthorityAccess {
  Unavailable,
  ReadOnly,
  Writable,
};

// These views borrow their strings only for the duration of the reducer call.
// The current tuple must be one atomically retained observation; the reducer
// still validates its public identifier and digest grammar before comparison.
struct CurrentAuthorityCasV2View final {
  AuthorityAccess access = AuthorityAccess::Unavailable;
  QStringView authorityId;
  quint64 revision = 0;
  QStringView catalogDigest;
  QStringView actionCatalogDigest;
};

struct ExpectedAuthorityCasV2View final {
  QStringView authorityId;
  quint64 revision = 0;
  QStringView catalogDigest;
  QStringView actionCatalogDigest;
};

enum class AuthorityCasDecision {
  Unavailable,
  ReadOnly,
  StaleAuthority,
  StaleCatalogDigest,
  StaleRevision,
  Proceed,
};

// Pure common gate for writable Compositor2 authority mutations. It performs
// no parsing, allocation, I/O, freshness check, replay lookup, or
// method-specific validation. Invalid access values or malformed current
// tuples fail as Unavailable. Malformed expected fields fail in the same class
// as a well-formed mismatch. The fixed precedence is:
//
// Unavailable -> ReadOnly -> StaleAuthority -> StaleCatalogDigest
// -> StaleRevision -> Proceed.
[[nodiscard]] AuthorityCasDecision
reduceAuthorityCasV2(const CurrentAuthorityCasV2View &current,
                     const ExpectedAuthorityCasV2View &expected);

} // namespace HyprShelld::Compositor
