/*
**
* BEGIN_COPYRIGHT
*
* Copyright (C) 2008-2015 SciDB, Inc.
* All Rights Reserved.
*
* SciDB is free software: you can redistribute it and/or modify
* it under the terms of the AFFERO GNU General Public License as published by
* the Free Software Foundation.
*
* SciDB is distributed "AS-IS" AND WITHOUT ANY WARRANTY OF ANY KIND,
* INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,
* NON-INFRINGEMENT, OR FITNESS FOR A PARTICULAR PURPOSE. See
* the AFFERO GNU General Public License for the complete license terms.
*
* You should have received a copy of the AFFERO GNU General Public License
* along with SciDB.  If not, see <http://www.gnu.org/licenses/agpl-3.0.html>
*
* END_COPYRIGHT
*/

/*
 * @file LogicalExplainPhysical.cpp
 *
 * @author poliocough@gmail.com
 *
 * explain_physical operator / Physical implementation.
 */

#include <string.h>

#include "query/Operator.h"
#include "query/OperatorLibrary.h"
#include "array/TupleArray.h"
#include "query/QueryProcessor.h"
#include "query/optimizer/Optimizer.h"
#include <util/Thread.h>
#include "SciDBAPI.h"

using namespace std;
using namespace boost;

namespace scidb
{

class PhysicalExplainPhysical: public PhysicalOperator
{
public:
    PhysicalExplainPhysical(const string& logicalName, const string& physicalName, const Parameters& parameters, const ArrayDesc& schema):
        PhysicalOperator(logicalName, physicalName, parameters, schema)
    {}

    virtual RedistributeContext getOutputDistribution(const std::vector<RedistributeContext> & inputDistributions,
                                                 const std::vector< ArrayDesc> & inputSchemas) const
    {
        return RedistributeContext(psLocalInstance);
    }

    void preSingleExecute(std::shared_ptr<Query> query)
    {
        bool afl = false;

        assert (_parameters.size()==1 || _parameters.size()==2);
        string queryString = ((std::shared_ptr<OperatorParamPhysicalExpression>&)_parameters[0])->getExpression()->evaluate().getString();

        if (_parameters.size() == 2)
        {
                string languageSpec = ((std::shared_ptr<OperatorParamPhysicalExpression>&)_parameters[1])->getExpression()->evaluate().getString();
                if (languageSpec == "afl")
                {       afl = true; }
        }

        std::shared_ptr<QueryProcessor> queryProcessor = QueryProcessor::create();
        std::shared_ptr<Query> innerQuery = Query::createFakeQuery(
                         query->getPhysicalCoordinatorID(),
                         query->mapLogicalToPhysical(query->getInstanceID()),
                         query->getCoordinatorLiveness());

        boost::function<void()> func = boost::bind(&Query::destroyFakeQuery, innerQuery.get());
        Destructor<boost::function<void()> > fqd(func);

        innerQuery->queryString = queryString;
        queryProcessor->parseLogical(innerQuery, afl);
        queryProcessor->inferTypes(innerQuery);

        std::shared_ptr< Optimizer> optimizer =  Optimizer::create();
        queryProcessor->optimize(optimizer, innerQuery);

        std::ostringstream planString;
        //TODO: Maybe here it's better to print every plan
        innerQuery->getCurrentPhysicalPlan()->toString(planString);

        std::shared_ptr<TupleArray> tuples(std::make_shared<TupleArray>(_schema, _arena));
        Value tuple[1];
        tuple[0].setData(planString.str().c_str(), planString.str().length() + 1);
        tuples->appendTuple(tuple);
        _result = tuples;
    }

   std::shared_ptr<Array> execute(vector< std::shared_ptr<Array> >& inputArrays, std::shared_ptr<Query> query)
   {
       if (!_result)
       {
           _result = std::make_shared<MemArray>(_schema, query);
       }
       return _result;
   }

private:
   std::shared_ptr<Array> _result;
};

DECLARE_PHYSICAL_OPERATOR_FACTORY(PhysicalExplainPhysical, "_explain_physical", "physicalExplainPhysical")

} //namespace
