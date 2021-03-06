/*
* Copyright (c) 2007, Sandia Corporation
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the Sandia Corporation nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY Sandia Corporation ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL Sandia Corporation BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "ClientRecordView.h"
#include "ClientRecordViewDecorator.h"

#include <vtkAnnotationLink.h>
#include <vtkCommand.h>
#include <vtkConvertSelection.h>
#include <vtkDataArray.h>
#include <vtkDataArrayTemplate.h>
#include <vtkDataObject.h>
#include <vtkDataObjectToTable.h>
#include <vtkDataObjectTypes.h>
#include <vtkDataRepresentation.h>
#include <vtkDataSetAttributes.h>
#include <vtkGraph.h>
#include <vtkIdList.h>
#include <vtkIdTypeArray.h>
#include <vtkPVDataInformation.h>
#include <vtkQtRecordView.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>
#include <vtkSMSelectionDeliveryRepresentationProxy.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkStringArray.h>
#include <vtkTable.h>
#include <vtkVariant.h>
#include <vtkVariantArray.h>

#include <pqDataRepresentation.h>
#include <pqOutputPort.h>
#include <pqPipelineSource.h>
#include <pqRepresentation.h>
#include <pqSelectionManager.h>
#include <pqServer.h>

#include <QPointer>
#include <QVBoxLayout>
#include <QWidget>

////////////////////////////////////////////////////////////////////////////////////
// ClientTableView::command

class ClientRecordView::command : public vtkCommand
{
public:
  command(ClientRecordView& view) : Target(view) { }
  virtual void Execute(vtkObject*, unsigned long, void*)
  {
    Target.selectionChanged();
  }
  ClientRecordView& Target;
};

////////////////////////////////////////////////////////////////////////////////////
// ClientRecordView::implementation

class ClientRecordView::implementation
{
public:
  implementation()
  {
  this->Widget = new QWidget();
  this->View = vtkSmartPointer<vtkQtRecordView>::New();
  QVBoxLayout *layout = new QVBoxLayout(this->Widget);
  layout->addWidget(this->View->GetWidget());
  layout->setContentsMargins(0,0,0,0);
  }

  ~implementation()
  {
    if(this->Widget)
      delete this->Widget;
    this->View->RemoveAllRepresentations();
  }

  vtkSmartPointer<vtkQtRecordView> View;
  QPointer<QWidget> Widget;
};

////////////////////////////////////////////////////////////////////////////////////
// ClientRecordView

ClientRecordView::ClientRecordView(
    const QString& viewmoduletype, 
    const QString& group, 
    const QString& name, 
    vtkSMViewProxy* viewmodule, 
    pqServer* server, 
    QObject* p) :
  pqSingleInputView(viewmoduletype, group, name, viewmodule, server, p),
  Implementation(new implementation()),
  Command(new command(*this))
{
  this->Implementation->View->AddObserver(
    vtkCommand::SelectionChangedEvent, this->Command);

  new ClientRecordViewDecorator(this);
}

ClientRecordView::~ClientRecordView()
{
  delete this->Implementation;
  this->Command->Delete();
}

void ClientRecordView::selectionChanged()
{
  // Get the representaion's source
  pqDataRepresentation* pqRepr =
    qobject_cast<pqDataRepresentation*>(this->visibleRepresentation());
  pqOutputPort* opPort = pqRepr->getOutputPortFromInput();
  vtkSMSourceProxy* repSource = vtkSMSourceProxy::SafeDownCast(
    opPort->getSource()->getProxy());

  // Fill the selection source with the selection from the view
  vtkSelection* sel = this->Implementation->View->GetRepresentation()->
    GetAnnotationLink()->GetCurrentSelection();
  vtkSMSourceProxy* selectionSource = pqSelectionManager::createSelectionSource(
    sel, repSource->GetConnectionID());

  // Set the selection on the representation's source
  repSource->SetSelectionInput(opPort->getPortNumber(),
    selectionSource, 0);
  selectionSource->Delete();

  // Mark the annotation link as modified so it will be updated
  if (this->getAnnotationLink())
    {
    this->getAnnotationLink()->MarkModified(0);
    }
}

QWidget* ClientRecordView::getWidget()
{
  return this->Implementation->Widget;
}

bool ClientRecordView::canDisplay(pqOutputPort* output_port) const
{
  if(!output_port)
    return false;

  pqPipelineSource* const source = output_port->getSource();
  if(!source)
    return false;

  if(this->getServer()->GetConnectionID() != source->getServer()->GetConnectionID())
    return false;

  vtkSMSourceProxy* source_proxy =
    vtkSMSourceProxy::SafeDownCast(source->getProxy());
  if (!source_proxy ||
     source_proxy->GetOutputPortsCreated() == 0)
    {
    return false;
    }

  const char* name = output_port->getDataClassName();
  int type = vtkDataObjectTypes::GetTypeIdFromClassName(name);
  switch(type)
    {    
    case VTK_DIRECTED_ACYCLIC_GRAPH:
    case VTK_DIRECTED_GRAPH:
    case VTK_GRAPH:
    case VTK_TREE:
    case VTK_UNDIRECTED_GRAPH:
    case VTK_TABLE:
      return true;
    }

  return false;
}

void ClientRecordView::updateRepresentation(pqRepresentation* repr)
{
  vtkSMSelectionDeliveryRepresentationProxy* const proxy = repr ? 
    vtkSMSelectionDeliveryRepresentationProxy::SafeDownCast(repr->getProxy()) : NULL;
  proxy->Update(vtkSMViewProxy::SafeDownCast(this->getProxy()));  

  vtkDataObject *data = proxy? vtkDataObject::SafeDownCast(proxy->GetOutput()) : NULL;
  if (!data)
    {
    return;
    }

  // Add the representation to the view
  vtkDataRepresentation* rep = this->Implementation->View->SetRepresentationFromInputConnection(proxy->GetOutput()->GetProducerPort());
  rep->SetSelectionType(vtkSelectionNode::PEDIGREEIDS);
  // If we have an associated annotation link proxy, set the client side
  // object as the annotation link on the representation.
  if (this->getAnnotationLink())
    {
    vtkAnnotationLink* link = static_cast<vtkAnnotationLink*>(this->getAnnotationLink()->GetClientSideObject());
    rep->SetAnnotationLink(link);
    }
}


void ClientRecordView::showRepresentation(pqRepresentation* representation)
{
  this->updateRepresentation(representation);
}

void ClientRecordView::hideRepresentation(pqRepresentation* representation)
{
  // Because this view can only take one representation, we can do this, and thus
  // not keep track of the vtk reprsentations
  this->Implementation->View->RemoveAllRepresentations();
}

void ClientRecordView::renderInternal()
{
  pqRepresentation* representation = this->visibleRepresentation();
  vtkSMSelectionDeliveryRepresentationProxy* const proxy = representation ?
    vtkSMSelectionDeliveryRepresentationProxy::SafeDownCast(representation->getProxy()) : NULL;

  if(!proxy)
    {
    return;
    }

  proxy->Update();

  int attributeType = QString(vtkSMPropertyHelper(proxy, "AttributeType").GetAsString(3)).toInt();

  if (attributeType == vtkDataObject::FIELD_ASSOCIATION_EDGES)
    {
    this->Implementation->View->SetFieldType(vtkQtRecordView::EDGE_DATA);
    }
  else if(attributeType == vtkDataObject::FIELD_ASSOCIATION_ROWS)
    {
    this->Implementation->View->SetFieldType(vtkQtRecordView::ROW_DATA);
    }
  else
    {
    this->Implementation->View->SetFieldType(vtkQtRecordView::VERTEX_DATA);
    }

  vtkDataRepresentation *rep = this->Implementation->View->GetRepresentation();
  if(rep && !this->getAnnotationLink())
    {
    // Only use the source proxy's selection if we're not using vtkAnnotationLink directly
    proxy->GetSelectionRepresentation()->Update();
    vtkSelection* sel = vtkSelection::SafeDownCast(
      proxy->GetSelectionRepresentation()->GetOutput());
    rep->GetAnnotationLink()->SetCurrentSelection(sel);
    }

  this->Implementation->View->Update();
}
