/***************************************************************************
 *                                                                         *
 * Copyright (C) 2015 by frePPLe bvba                                      *
 *                                                                         *
 * This library is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU Affero General Public License as published   *
 * by the Free Software Foundation; either version 3 of the License, or    *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This library is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
 * GNU Affero General Public License for more details.                     *
 *                                                                         *
 * You should have received a copy of the GNU Affero General Public        *
 * License along with this program.                                        *
 * If not, see <http://www.gnu.org/licenses/>.                             *
 *                                                                         *
 ***************************************************************************/

#define FREPPLE_CORE
#include "frepple/model.h"

namespace frepple
{

DECLARE_EXPORT const MetaCategory* ItemSupplier::metacategory;
DECLARE_EXPORT const MetaClass* ItemSupplier::metadata;
DECLARE_EXPORT const MetaClass* OperationItemSupplier::metadata;


int ItemSupplier::initialize()
{
  // Initialize the metadata
  metacategory = MetaCategory::registerCategory<ItemSupplier>(
	  "itemsupplier", "itemsuppliers",
    Association<Supplier,Item,ItemSupplier>::reader, finder
	  );
  metadata = MetaClass::registerClass<ItemSupplier>(
    "itemsupplier", "itemsupplier", Object::create<ItemSupplier>, true
  );
  registerFields<ItemSupplier>(const_cast<MetaClass*>(metadata));

  // Initialize the Python class
  PythonType& x = FreppleCategory<ItemSupplier>::getPythonType();
  x.setName("itemsupplier");
  x.setDoc("frePPLe itemsupplier");
  x.supportgetattro();
  x.supportsetattro();
  x.supportcreate(create);
  x.addMethod("toXML", toXML, METH_VARARGS, "return a XML representation");
  const_cast<MetaClass*>(ItemSupplier::metadata)->pythonClass = x.type_object();
  return x.typeReady();
}


DECLARE_EXPORT ItemSupplier::~ItemSupplier()
{
  // Delete the association from the related objects
  if (getSupplier())
    getSupplier()->items.erase(this);
  if (getItem())
    getItem()->suppliers.erase(this);

  // Delete all owned purchase operations
  while (firstOperation)
    delete firstOperation;

  // Trigger level and cluster recomputation
  HasLevel::triggerLazyRecomputation();
}


DECLARE_EXPORT ItemSupplier::ItemSupplier() :
  loc(nullptr), size_minimum(1.0), size_multiple(1.0), cost(0.0),
  firstOperation(nullptr), res(nullptr), res_qty(1.0)
{
  initType(metadata);

  // Trigger level and cluster recomputation
  HasLevel::triggerLazyRecomputation();
}


DECLARE_EXPORT ItemSupplier::ItemSupplier(Supplier* s, Item* r, int u) :
  loc(nullptr), size_minimum(1.0), size_multiple(1.0), cost(0.0),
  firstOperation(nullptr), res(nullptr), res_qty(1.0)
{
  setSupplier(s);
  setItem(r);
  setPriority(u);
  initType(metadata);

  // Trigger level and cluster recomputation
  HasLevel::triggerLazyRecomputation();
}


DECLARE_EXPORT ItemSupplier::ItemSupplier(Supplier* s, Item* r, int u, DateRange e) :
  loc(nullptr), size_minimum(1.0), size_multiple(1.0), cost(0.0),
  firstOperation(nullptr), res(nullptr), res_qty(1.0)
{
  setSupplier(s);
  setItem(r);
  setPriority(u);
  setEffective(e);
  initType(metadata);

  // Trigger level and cluster recomputation
  HasLevel::triggerLazyRecomputation();
}


PyObject* ItemSupplier::create(PyTypeObject* pytype, PyObject* args, PyObject* kwds)
{
  try
  {
    // Pick up the supplier
    PyObject* sup = PyDict_GetItemString(kwds,"supplier");
    if (!sup)
      throw DataException("missing supplier on ItemSupplier");
    if (!PyObject_TypeCheck(sup, Supplier::metadata->pythonClass))
      throw DataException("ItemSupplier supplier must be of type supplier");

    // Pick up the item
    PyObject* it = PyDict_GetItemString(kwds,"item");
    if (!it)
      throw DataException("missing item on ItemSupplier");
    if (!PyObject_TypeCheck(it, Item::metadata->pythonClass))
      throw DataException("ItemSupplier item must be of type item");

    // Pick up the priority
    PyObject* q1 = PyDict_GetItemString(kwds,"priority");
    int q2 = q1 ? PythonData(q1).getInt() : 1;

    // Pick up the effective dates
    DateRange eff;
    PyObject* eff_start = PyDict_GetItemString(kwds,"effective_start");
    if (eff_start)
    {
      PythonData d(eff_start);
      eff.setStart(d.getDate());
    }
    PyObject* eff_end = PyDict_GetItemString(kwds,"effective_end");
    if (eff_end)
    {
      PythonData d(eff_end);
      eff.setEnd(d.getDate());
    }

    // Create the ItemSupplier
    ItemSupplier *l = new ItemSupplier(
      static_cast<Supplier*>(sup),
      static_cast<Item*>(it),
      q2, eff
    );

    // Iterate over extra keywords, and set attributes.   @todo move this responsibility to the readers...
    if (l)
    {
      PyObject *key, *value;
      Py_ssize_t pos = 0;
      while (PyDict_Next(kwds, &pos, &key, &value))
      {
        PythonData field(value);
        PyObject* key_utf8 = PyUnicode_AsUTF8String(key);
        DataKeyword attr(PyBytes_AsString(key_utf8));
        Py_DECREF(key_utf8);
        if (!attr.isA(Tags::effective_end) && !attr.isA(Tags::effective_start)
          && !attr.isA(Tags::supplier) && !attr.isA(Tags::item)
          && !attr.isA(Tags::type) && !attr.isA(Tags::priority) && !attr.isA(Tags::action))
        {
          const MetaFieldBase* fmeta = l->getType().findField(attr.getHash());
          if (!fmeta && l->getType().category)
            fmeta = l->getType().category->findField(attr.getHash());
          if (fmeta)
            // Update the attribute
            fmeta->setField(l, field);
          else
            l->setProperty(attr.getName(), value);
        }
      };
    }

    // Return the object
    Py_INCREF(l);
    return static_cast<PyObject*>(l);
  }
  catch (...)
  {
    PythonType::evalException();
    return nullptr;
  }
}


DECLARE_EXPORT void ItemSupplier::deleteOperationPlans(bool b)
{
  for (OperationItemSupplier* i = firstOperation; i; i = i->nextOperation)
    i->deleteOperationPlans(b);
}


int OperationItemSupplier::initialize()
{
  // Initialize the metadata
  metadata = MetaClass::registerClass<OperationItemSupplier>(
    "operation", "operation_itemsupplier"
    );
  registerFields<OperationItemSupplier>(const_cast<MetaClass*>(metadata));

  // Initialize the Python class
  PythonType& x = FreppleCategory<OperationItemSupplier>::getPythonType();
  x.setName("operation_itemsupplier");
  x.setDoc("frePPLe operation_itemsupplier");
  x.addMethod("createOrder", createOrder,
    METH_STATIC | METH_VARARGS | METH_KEYWORDS,
    "Create an operationplan representing a purchase order");
  x.supportgetattro();
  x.supportsetattro();
  const_cast<MetaClass*>(metadata)->pythonClass = x.type_object();
  return x.typeReady();
}


DECLARE_EXPORT OperationItemSupplier* OperationItemSupplier::findOrCreate(
  ItemSupplier* i, Buffer *b
  )
{
  if (!i || !b || !i->getSupplier())
    throw LogicException(
      "An OperationItemSupplier always needs to point to "
      "a itemsupplier and a buffer"
      );
  stringstream o;
  o << "Purchase " << b->getName() << " from " << i->getSupplier()->getName();
  Operation *oper = Operation::find(o.str());
  if (oper)
  {
    // Reuse existing operation
    if (oper->getType() == *OperationItemSupplier::metadata)
      return static_cast<OperationItemSupplier*>(oper);
    else
      throw DataException("Unexpected operation type for item supplier operation");
  }
  else
    // Create new operation
    return new OperationItemSupplier(i, b);
}


DECLARE_EXPORT OperationItemSupplier::OperationItemSupplier(
  ItemSupplier* i, Buffer *b
  ) : supitem(i)
{
  if (!i || !b || !i->getSupplier())
    throw LogicException(
      "An OperationItemSupplier always needs to point to "
      "a itemsupplier and a buffer"
      );
  stringstream o;
  o << "Purchase " << b->getName() << " from " << i->getSupplier()->getName();
  setName(o.str());
  setDuration(i->getLeadTime());
  setSizeMultiple(i->getSizeMultiple());
  setSizeMinimum(i->getSizeMinimum());
  setLocation(b->getLocation());
  setSource(i->getSource());
  setCost(i->getCost());
  setFence(i->getFence());
  setHidden(true);
  new FlowEnd(this, b, 1);
  initType(metadata);

  // Optionally, create a load
  if (i->getResource())
    new LoadDefault(this, i->getResource(), i->getResourceQuantity());

  // Insert in the list of ItemSupplier operations.
  // We keep the list sorted by the operation name.
  if (!i->firstOperation || getName() < i->firstOperation->getName())
  {
    // New head of the list
    nextOperation = i->firstOperation;
    i->firstOperation = this;
  }
  else
  {
    // Insert in the middle or at the tail
    OperationItemSupplier* o = i->firstOperation;
    while (o->nextOperation)
    {
      if (b->getName() < o->nextOperation->getName())
        break;
      o = o->nextOperation;
    }
    nextOperation = o->nextOperation;
    o->nextOperation = this;
  }
}


OperationItemSupplier::~OperationItemSupplier()
{
  // Remove from the list of operations of this supplier item
  if (supitem)
  {
    if (supitem->firstOperation == this)
    {
      // We were at the head
      supitem->firstOperation = nextOperation;
    }
    else
    {
      // We were in the middle
      OperationItemSupplier* i = supitem->firstOperation;
      while (i->nextOperation != this && i->nextOperation)
        i = i->nextOperation;
      if (!i)
        logger << "Error: ItemSupplier operation list corrupted" << endl;
      else
        i->nextOperation = nextOperation;
    }
  }
}


DECLARE_EXPORT Buffer* OperationItemSupplier::getBuffer() const
{
  return getFlows().begin()->getBuffer();
}


DECLARE_EXPORT void OperationItemSupplier::trimExcess(bool zero_or_minimum) const
{
  // This method can only trim operations not loading a resource
  if (getLoads().begin() != getLoads().end())
    return;

  for (Operation::flowlist::const_iterator fliter = getFlows().begin();
    fliter != getFlows().end(); ++fliter)
  {
    if (fliter->getQuantity() <= 0)
      // Strange, shouldn't really happen
      continue;
    FlowPlan* candidate = nullptr;
    double curmin = 0;
    double oh = 0;
    double excess_min = DBL_MAX;

    for (Buffer::flowplanlist::const_iterator flplniter = fliter->getBuffer()->getFlowPlans().begin();
      flplniter != fliter->getBuffer()->getFlowPlans().end();
      ++flplniter)
    {
      // For any operationplan we get the onhand when its successor
      // replenishment arrives. If that onhand is higher than the minimum
      // onhand value we can resize it.
      // This is only valid in unconstrained plans and when there are
      // no upstream activities.
      if (flplniter->getEventType() == 3 && zero_or_minimum)
        curmin = flplniter->getMin();
      else if (flplniter->getEventType() == 1)
      {
        const FlowPlan* flpln = static_cast<const FlowPlan*>(&*flplniter);
        if (oh - curmin < excess_min)
        {
          excess_min = oh - curmin;
          if (excess_min < 0)
            excess_min = 0;
        }
        if (flpln->getQuantity() > 0 && !flpln->getOperationPlan()->getLocked() && (!candidate || candidate->getDate() != flpln->getDate()))
        {
          if (candidate
            && excess_min > ROUNDING_ERROR
            && candidate->getQuantity() > excess_min + ROUNDING_ERROR
            && candidate->getQuantity() > getSizeMinimum() + ROUNDING_ERROR
            )
          {
            // This candidate can now be resized
            candidate->setQuantity(candidate->getQuantity() - excess_min, false);
            candidate = nullptr;
          }
          else if (flpln->getOperation() == this)
            candidate = const_cast<FlowPlan*>(flpln);
          else
            candidate = nullptr;
          excess_min = DBL_MAX;
        }
      }
      oh = flplniter->getOnhand();
    }
    if (candidate
      && excess_min > ROUNDING_ERROR
      && candidate->getQuantity() > excess_min + ROUNDING_ERROR
      && candidate->getQuantity() > getSizeMinimum() + ROUNDING_ERROR
      )
      // Resize the last candidate at the end of the horizon
      candidate->setQuantity(candidate->getQuantity() - excess_min, false);
  }
}


extern "C" PyObject* OperationItemSupplier::createOrder(
  PyObject *self, PyObject *args, PyObject *kwdict
  )
{
  // Parse the Python arguments
  PyObject* pylocation = nullptr;
  unsigned long id = 0;
  const char* ref = nullptr;
  PyObject* pyitem = nullptr;
  PyObject* pysupplier = nullptr;
  double qty = 0;
  PyObject* pystart = nullptr;
  PyObject* pyend = nullptr;
  const char* status = nullptr;
  const char* source = nullptr;
  static const char *kwlist[] = {
    "location", "id", "reference", "item", "supplier", "quantity", "start",
    "end", "status", "source", nullptr
    };
  int ok = PyArg_ParseTupleAndKeywords(
    args, kwdict, "|OkzOOdOOzz:createOrder", const_cast<char**>(kwlist),
    &pylocation, &id, &ref, &pyitem, &pysupplier, &qty, &pystart,
    &pyend, &status, &source
    );
  if (!ok)
    return nullptr;
  Date start = pystart ? PythonData(pystart).getDate() : Date::infinitePast;
  Date end = pyend ? PythonData(pyend).getDate() : Date::infinitePast;

  // Validate all arguments
  if (!pylocation || !pyitem)
  {
    PyErr_SetString(PythonDataException, "item and location arguments are mandatory");
    return nullptr;
  }
  PythonData location_tmp(pylocation);
  if (!location_tmp.check(Location::metadata))
  {
    PyErr_SetString(PythonDataException, "location argument must be of type location");
    return nullptr;
  }
  PythonData item_tmp(pyitem);
  if (!item_tmp.check(Item::metadata))
  {
    PyErr_SetString(PythonDataException, "item argument must be of type item");
    return nullptr;
  }
  PythonData supplier_tmp(pysupplier);
  if (pysupplier && !supplier_tmp.check(Supplier::metadata))
  {
    PyErr_SetString(PythonDataException, "supplier argument must be of type supplier");
    return nullptr;
  }
  Item *item = static_cast<Item*>(item_tmp.getObject());
  Location *location = static_cast<Location*>(location_tmp.getObject());
  Supplier *supplier = pysupplier ? static_cast<Supplier*>(supplier_tmp.getObject()) : nullptr;

  // Find or create the destination buffer.
  Buffer* destbuffer = nullptr;
  Item::bufferIterator buf_iter(item);
  while (Buffer* tmpbuf = buf_iter.next())
  {
    if (tmpbuf->getLocation() == location)
    {
      if (destbuffer)
      {
        stringstream o;
        o << "Multiple buffers found for item '" << item << "'' and location'" << location << "'";
        throw DataException(o.str());
      }
      destbuffer = tmpbuf;
    }
  }
  if (!destbuffer)
  {
    // Create the destination buffer
    destbuffer = new BufferDefault();
    stringstream o;
    o << item << " @ " << location;
    destbuffer->setName(o.str());
    destbuffer->setItem(item);
    destbuffer->setLocation(location);
  }

  // Build the producing operation for this buffer.
  destbuffer->getProducingOperation();

  // Look for a matching operation replenishing this buffer.
  Operation *oper = nullptr;
  for (Buffer::flowlist::const_iterator flowiter = destbuffer->getFlows().begin();
    flowiter != destbuffer->getFlows().end() && !oper; ++flowiter)
  {
    if (flowiter->getOperation()->getType() != *OperationItemSupplier::metadata)
      continue;
    OperationItemSupplier* opitemsupplier = static_cast<OperationItemSupplier*>(flowiter->getOperation());
    if (supplier)
    {
      if (supplier->isMemberOf(opitemsupplier->getItemSupplier()->getSupplier()))
        oper = opitemsupplier;
    }
    else
      oper = opitemsupplier;
  }

  // No matching operation is found.
  OperationPlan *opplan = nullptr;
  if (!oper)
  {
    // We'll create one now, but that requires that we have a supplier defined.
    if (!supplier)
      throw DataException("Supplier is needed on this purchase order");
    // Note: We know that we need to create a new one. An existing one would
    // have created an operation on the buffer already.
    ItemSupplier *itemsupplier = new ItemSupplier();
    itemsupplier->setSupplier(supplier);
    itemsupplier->setItem(item);
    itemsupplier->setLocation(location);
    itemsupplier->setHidden(true);
    oper = new OperationItemSupplier(itemsupplier, destbuffer);
    // Create operation plan
    opplan = oper->createOperationPlan(qty, start, end);
    new ProblemInvalidData(opplan, "Purchase orders on unauthorized supplier", "operation",
      start, end, qty);
  }
  else
    // Create the operationplan
    opplan = oper->createOperationPlan(qty, start, end);

  // Set operationplan fields
  if (id)
    opplan->setRawIdentifier(id);  // We can use this fast method because we call activate later
  if (status)
    opplan->setStatus(status);
  // Reset quantity after the status update to assure that
  // also non-valid quantities are getting accepted.
  opplan->setQuantity(qty);
  if (ref)
    opplan->setReference(ref);
  opplan->activate();

  // Return result
  Py_INCREF(opplan);
  return opplan;
}


DECLARE_EXPORT Object* ItemSupplier::finder(const DataValueDict& d)
{
  // Check item
  const DataValue* tmp = d.get(Tags::item);
  if (!tmp)
    return nullptr;
  Item* item = static_cast<Item*>(tmp->getObject());

  // Check supplier field
  tmp = d.get(Tags::supplier);
  if (!tmp)
    return nullptr;
  Supplier* sup = static_cast<Supplier*>(tmp->getObject());

  // Walk over all suppliers of the item, and return
  // the first one with matching
  const DataValue* hasEffectiveStart = d.get(Tags::effective_start);
  Date effective_start;
  if (hasEffectiveStart)
    effective_start = hasEffectiveStart->getDate();
  const DataValue* hasEffectiveEnd = d.get(Tags::effective_end);
  Date effective_end;
  if (hasEffectiveEnd)
    effective_end = hasEffectiveEnd->getDate();
  const DataValue* hasPriority = d.get(Tags::priority);
  int priority;
  if (hasPriority)
    priority = hasPriority->getInt();
  for (Item::supplierlist::const_iterator fl = item->getSuppliers().begin();
    fl != item->getSuppliers().end(); ++fl)
  {
    if (fl->getSupplier() != sup)
      continue;
    if (hasEffectiveStart && fl->getEffectiveStart() != effective_start)
      continue;
    if (hasEffectiveEnd && fl->getEffectiveEnd() != effective_end)
      continue;
    if (hasPriority && fl->getPriority() != priority)
      continue;
    return const_cast<ItemSupplier*>(&*fl);
  }
  return nullptr;
}

}
