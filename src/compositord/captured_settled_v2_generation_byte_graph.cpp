#include "captured_settled_v2_generation_byte_graph.h"

namespace HyprShelld::Compositor {
namespace {

[[nodiscard]] QByteArrayView
requiredView(const DormantFixedRecordField &field) {
  switch (field.kind()) {
  case DormantFixedRecordFieldKind::Missing:
    return {};
  case DormantFixedRecordFieldKind::PresentBytes:
    return QByteArrayView(field.bytes());
  }
  return {};
}

[[nodiscard]] std::optional<QByteArrayView>
optionalView(const DormantFixedRecordField &field) {
  switch (field.kind()) {
  case DormantFixedRecordFieldKind::Missing:
    return std::nullopt;
  case DormantFixedRecordFieldKind::PresentBytes:
    return QByteArrayView(field.bytes());
  }
  // An unknown future kind must remain present-invalid rather than becoming
  // an observation of absence.
  return QByteArrayView{};
}

} // namespace

SettledV2GenerationByteGraphResult
classifyCapturedSettledV2GenerationContentByteGraph(
    const DormantFixedRecordCapture &capture,
    const QVector<SettledV2GenerationEvidence> &evidence,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2) {
  const SettledV2CurrentRecordBytes current{
      .authority = requiredView(capture.authority()),
      .desired = requiredView(capture.desired()),
      .lastGood = optionalView(capture.lastGood()),
      .applied = optionalView(capture.applied()),
  };
  const auto pending = optionalView(capture.pending());
  return classifySettledV2GenerationContentByteGraph(
      current, pending, evidence, catalogV2, actionCatalogV2);
}

} // namespace HyprShelld::Compositor
