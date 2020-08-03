/*
 * material_descriptor.h
 *
 *  Created on: 21.03.2020
 *      Author: fehn
 */

#ifndef INCLUDE_STRUCTURE_USER_INTERFACE_MATERIAL_DESCRIPTOR_H_
#define INCLUDE_STRUCTURE_USER_INTERFACE_MATERIAL_DESCRIPTOR_H_

// C/C++
#include <map>

// deal.II
#include <deal.II/base/types.h>

#include "../material/material_data.h"

using namespace dealii;

namespace Structure
{
using MaterialDescriptor = std::map<types::material_id, std::shared_ptr<MaterialData>>;
}

#endif
