/*
 *  environment/Resource.h
 *  avida-core
 *
 *  Created by David on 6/24/11.
 *  Copyright 2011 Michigan State University. All rights reserved.
 *  http://avida.devosoft.org/
 *
 *
 *  This file is part of Avida.
 *
 *  Avida is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  Avida is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License along with Avida.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Authors: David M. Bryson <david@programerror.com>
 *
 */

#ifndef AvidaEnvironmentResource_h
#define AvidaEnvironmentResource_h


namespace Avida {
  namespace Environment {
    
    // Environment::Resource
    // --------------------------------------------------------------------------------------------------------------
    
    class Resource
    {
    private:
      const ResourceID m_id;
      
    public:
      Resource(const ResourceID& resource_id);
      
      const ResourceID& GetID() const { return m_id; }
    };
    
  };
};

#endif
