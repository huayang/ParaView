/*=========================================================================

  Program:   ParaView
  Module:    vtkSMSourceProxy.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkSMSourceProxy - proxy for a VTK source on a server
// .SECTION Description
// vtkSMSourceProxy manages VTK source(s) that are created on a server
// using the proxy pattern. In addition to functionality provided
// by vtkSMProxy, vtkSMSourceProxy provides method to connect and
// update sources. Each source proxy has one or more output ports 
// (vtkSMOutputPort).
// Each port represents one output of one filter. These are created
// automatically (when CreateOutputPorts() is called) by the source.
// Each vtkSMSourceProxy creates a property called DataInformation.
// This property is a composite property that provides information
// about the output(s) of the VTK sources (obtained from the server)
// .SECTION See Also
// vtkSMProxy vtkSMOutputPort vtkSMInputProperty

#ifndef __vtkSMSourceProxy_h
#define __vtkSMSourceProxy_h

#include "vtkSMProxy.h"
#include "vtkClientServerID.h" // Needed for ClientServerID

class vtkPVArrayInformation;
class vtkPVDataInformation;
class vtkPVDataSetAttributesInformation;
//BTX
struct vtkSMSourceProxyInternals;
//ETX
class vtkSMOutputPort;
class vtkSMProperty;

class VTK_EXPORT vtkSMSourceProxy : public vtkSMProxy
{
public:
  static vtkSMSourceProxy* New();
  vtkTypeMacro(vtkSMSourceProxy, vtkSMProxy);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Calls UpdateInformation() on all sources.
  virtual void UpdatePipelineInformation();

  // Description:
  // Calls Update() on all sources. It also creates output ports if
  // they are not already created.
  virtual void UpdatePipeline();

  // Description:
  // Calls Update() on all sources with the given time request. 
  // It also creates output ports if they are not already created.
  virtual void UpdatePipeline(double time);

  // Description:
  // Connects filters/sinks to an input. If the filter(s) is not
  // created, this will create it. 
  virtual void AddInput(unsigned int inputPort,
                        vtkSMSourceProxy* input,
                        unsigned int outputPort,
                        const char* method);
  virtual void AddInput(vtkSMSourceProxy* input,
                        const char* method)
  {
    this->AddInput(0, input, 0, method);
  }
  
  // Description:
  // Calls method on all VTK sources. Used by the input property 
  // to remove inputs. Made public to allow access by wrappers. Do
  // not use.
  void CleanInputs(const char* method);

  // Description:
  // Chains to superclass and calls UpdateInformation()
  virtual void UpdateSelfAndAllInputs();

  // Description:
  // Returns if the output port proxies have been created.
  vtkGetMacro(OutputPortsCreated, int);

  // Description:
  // Return the number of output ports.
  unsigned int GetNumberOfOutputPorts();

  // Description:
  // Return an output port.
  vtkSMOutputPort* GetOutputPort(unsigned int idx);

  // Description:
  // Return an output port, given the name for the port.
  // Each output port is assigned a unique name (either using the xml
  // configuration or automatically). The automatically assigned names are of
  // the type Output0, Output1 etc.
  vtkSMOutputPort* GetOutputPort(const char* portname);

  // Description:
  // Returns the port index, given the name of an output port.
  // Each output port is assigned a unique name (either using the xml
  // configuration or automatically). The automatically assigned names are of
  // the type Output0, Output1 etc.
  // Returns VTK_UNSIGNED_INT_MAX (i.e. ~0u) when a port with given port name
  // does not exist.
  unsigned int GetOutputPortIndex(const char* portname);

  // Description:
  // Returns the output port name given its index.
  // Each output port is assigned an unique name (either using the xml
  // configuration or automatically). The automatically assigned names are of
  // the type Output-0, Output-1 etc.
  const char* GetOutputPortName(unsigned int index);

  // Description:
  // It is possible to provide some documentation for each output port in the
  // configuraton xml. These methods provide access to the port specific
  // documentation, if any. If no documentation is present, these methods will
  // return 0.
  vtkSMDocumentation* GetOutputPortDocumentation(unsigned int index);
  vtkSMDocumentation* GetOutputPortDocumentation(const char* portname);

  // Description:
  // Creates the output port proxiess for this filter. 
  // Each output port proxy corresponds to an actual output port on the
  // algorithm.
  virtual void CreateOutputPorts();

  // Description:
  // DataInformation is used by the source proxy to obtain information
  // on the output(s) from the server. 
  // If \c update is false the pipeline will not 
  // be updated before gathering the data information.
  vtkPVDataInformation* GetDataInformation()
  {
    return this->GetDataInformation(0);
  }
  vtkPVDataInformation* GetDataInformation(unsigned int outputIdx);
    
  // Description:
  // This method saves state information about the proxy
  // which can be used to revive the proxy using server side objects
  // already present. This includes the entire state saved by calling 
  // SaveState() as well additional information such as server side
  // object IDs.
  // Overridden to save information pertinant to reviving the output ports.
  virtual vtkPVXMLElement* SaveRevivalState(vtkPVXMLElement* root);
  virtual int LoadRevivalState(vtkPVXMLElement* revivalElement);

  // Description:
  // Creates extract selection proxies for each output port if not already
  // created.
  void CreateSelectionProxies();

  // Description:
  // Set/Get the selection input. This is used to set the selection input to the
  // extarction proxy for the output port identified by \c portIndex.
  // If no extraction proxies are present, this method has no effect.
  void SetSelectionInput(unsigned int portIndex, vtkSMSourceProxy* input,
    unsigned int outputPort);
  
  // Description:
  // API to query selection input set using SetSelectionInput.
  vtkSMSourceProxy* GetSelectionInput(unsigned int portIndex);
  unsigned int GetSelectionInputPort(unsigned int portIndex);

  // Description:
  // Clean all selection inputs for the given port.
  void CleanSelectionInputs(unsigned int portIndex);

  // Description:
  // Returns the source proxy which provides the selected data from the given
  // output port.
  vtkSMSourceProxy* GetSelectionOutput(unsigned int portIndex);

  // Description:
  // This returns information about whether the VTK algorithm supports
  // multiple processes or not. SINGLE_PROCESS means that this algorithm
  // works only in serial, MULTIPLE_PROCESSES means that it will only
  // work in parallel (or it is useless in serial), BOTH means both :-)
  // Default is BOTH. This ivar is filled from the xml configuration.
  vtkGetMacro(ProcessSupport, int);

  // Description:
  // Returns the number of output ports provided by the algorithm.
  unsigned int GetNumberOfAlgorithmOutputPorts();

  // Description:
  // Returns the number of non-optional input ports required by the algorithm.
  // This value is cached after the first call.
  virtual unsigned int GetNumberOfAlgorithmRequiredInputPorts();


//BTX
  enum ProcessSupportType
  {
    SINGLE_PROCESS,
    MULTIPLE_PROCESSES,
    BOTH
  };

  // Description:
  // Set server ids on self and sub-proxies.
  // Overridden to set servers on the output ports.
  virtual void SetServers(vtkTypeUInt32 servers);
  
  // Description:
  // Marks the selection proxies dirty as well as chain to superclass.
  virtual void MarkDirty(vtkSMProxy* modifiedProxy);

protected:
  vtkSMSourceProxy();
  ~vtkSMSourceProxy();

  friend class vtkSMInputProperty;

  int OutputPortsCreated; 

  int ProcessSupport;

  // After the algorithm executes, this will be set to false.
  // Once a client fetches new data using GetDataInformation(),
  // this will be set to true.
  bool DataInformationValid;
  
  // Description:
  // Mark the data information as invalid.
  void InvalidateDataInformation();

  // Description:
  // Call superclass' and then assigns a new executive 
  // (vtkCompositeDataPipeline)
  virtual void CreateVTKObjects();

  char *ExecutiveName;
  vtkSetStringMacro(ExecutiveName);

  // Description:
  // Read attributes from an XML element.
  virtual int ReadXMLAttributes(vtkSMProxyManager* pm, vtkPVXMLElement* element);

  // Description:
  // Internal method which creates the output port proxies using the proxy specified.
  void CreateOutputPortsInternal(vtkSMProxy* op);

  // Description:
  // Method to set an output port at the given index. Provided for subclasses to
  // add output ports. It replaces the output port at the given index, if any,
  // The output ports will be resize to fit the specified index.
  void SetOutputPort(unsigned int index, const char* name, 
    vtkSMOutputPort* port, vtkSMDocumentation* doc);
  void RemoveAllOutputPorts();

  // Description:
  // Overwritten from superclass to invoke 
  virtual void PostUpdateData();
  
  int DoInsertExtractPieces;
  int SelectionProxiesCreated;

private:
  vtkSMSourceProxyInternals* PInternals;

  // used by GetNumberOfAlgorithmOutputPorts to cache the value to avoid
  // unnecessary information gathers.
  unsigned int NumberOfAlgorithmOutputPorts;
  unsigned int NumberOfAlgorithmRequiredInputPorts;

  vtkSMSourceProxy(const vtkSMSourceProxy&); // Not implemented
  void operator=(const vtkSMSourceProxy&); // Not implemented
//ETX
};

#endif
