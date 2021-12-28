/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_legacy_attribute_combine_xyz_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("X"));
  b.add_input<decl::Float>(N_("X"), "X_001");
  b.add_input<decl::String>(N_("Y"));
  b.add_input<decl::Float>(N_("Y"), "Y_001");
  b.add_input<decl::String>(N_("Z"));
  b.add_input<decl::Float>(N_("Z"), "Z_001");
  b.add_input<decl::String>(N_("Result"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiLayout *col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "input_type_x", 0, IFACE_("X"), ICON_NONE);
  uiItemR(col, ptr, "input_type_y", 0, IFACE_("Y"), ICON_NONE);
  uiItemR(col, ptr, "input_type_z", 0, IFACE_("Z"), ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeAttributeCombineXYZ *data = MEM_cnew<NodeAttributeCombineXYZ>(__func__);

  data->input_type_x = GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
  data->input_type_y = GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
  data->input_type_z = GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  NodeAttributeCombineXYZ *node_storage = (NodeAttributeCombineXYZ *)node->storage;
  update_attribute_input_socket_availabilities(
      *ntree, *node, "X", (GeometryNodeAttributeInputMode)node_storage->input_type_x);
  update_attribute_input_socket_availabilities(
      *ntree, *node, "Y", (GeometryNodeAttributeInputMode)node_storage->input_type_y);
  update_attribute_input_socket_availabilities(
      *ntree, *node, "Z", (GeometryNodeAttributeInputMode)node_storage->input_type_z);
}

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         const GeoNodeExecParams &params,
                                         StringRef result_name)
{
  /* Use the domain of the result attribute if it already exists. */
  std::optional<AttributeMetaData> result_info = component.attribute_get_meta_data(result_name);
  if (result_info) {
    return result_info->domain;
  }

  /* Otherwise use the highest priority domain from existing input attributes, or the default. */
  return params.get_highest_priority_input_domain({"X", "Y", "Z"}, component, ATTR_DOMAIN_POINT);
}

static void combine_attributes(GeometryComponent &component, const GeoNodeExecParams &params)
{
  const std::string result_name = params.get_input<std::string>("Result");
  if (result_name.empty()) {
    return;
  }
  const AttributeDomain result_domain = get_result_domain(component, params, result_name);

  OutputAttribute_Typed<float3> attribute_result =
      component.attribute_try_get_for_output_only<float3>(result_name, result_domain);
  if (!attribute_result) {
    return;
  }
  VArray<float> attribute_x = params.get_input_attribute<float>(
      "X", component, result_domain, 0.0f);
  VArray<float> attribute_y = params.get_input_attribute<float>(
      "Y", component, result_domain, 0.0f);
  VArray<float> attribute_z = params.get_input_attribute<float>(
      "Z", component, result_domain, 0.0f);

  for (const int i : IndexRange(attribute_result->size())) {
    const float x = attribute_x[i];
    const float y = attribute_y[i];
    const float z = attribute_z[i];
    attribute_result->set(i, {x, y, z});
  }
  attribute_result.save();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry::realize_instances_legacy(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    combine_attributes(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    combine_attributes(geometry_set.get_component_for_write<PointCloudComponent>(), params);
  }
  if (geometry_set.has<CurveComponent>()) {
    combine_attributes(geometry_set.get_component_for_write<CurveComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes::node_geo_legacy_attribute_combine_xyz_cc

void register_node_type_geo_attribute_combine_xyz()
{
  namespace file_ns = blender::nodes::node_geo_legacy_attribute_combine_xyz_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_LEGACY_ATTRIBUTE_COMBINE_XYZ,
                     "Attribute Combine XYZ",
                     NODE_CLASS_ATTRIBUTE,
                     0);
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  node_type_storage(
      &ntype, "NodeAttributeCombineXYZ", node_free_standard_storage, node_copy_standard_storage);

  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}