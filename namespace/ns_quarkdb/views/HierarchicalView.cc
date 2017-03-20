/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2016 CERN/Switzerland                                  *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 ************************************************************************/

#include "namespace/ns_quarkdb/views/HierarchicalView.hh"
#include "namespace/Constants.hh"
#include "namespace/interface/IContainerMDSvc.hh"
#include "namespace/interface/IFileMDSvc.hh"
#include "namespace/ns_quarkdb/persistency/ContainerMDSvc.hh"
#include "namespace/utils/PathProcessor.hh"
#include <cerrno>
#include <ctime>

#ifdef __APPLE__
#define EBADFD 77
#endif

EOSNSNAMESPACE_BEGIN

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
HierarchicalView::HierarchicalView()
  : pContainerSvc(nullptr), pFileSvc(nullptr),
    pRoot(std::shared_ptr<IContainerMD>(nullptr))
{
  std::map<std::string, std::string> config;
  pQuotaStats = new QuotaStats(config);
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
HierarchicalView::~HierarchicalView()
{
  delete pQuotaStats;
}

//------------------------------------------------------------------------------
// Configure the view
//------------------------------------------------------------------------------
void
HierarchicalView::configure(const std::map<std::string, std::string>& config)
{
  if (pContainerSvc == nullptr) {
    MDException e(EINVAL);
    e.getMessage() << "Container MD Service was not set";
    throw e;
  }

  if (pFileSvc == nullptr) {
    MDException e(EINVAL);
    e.getMessage() << "File MD Service was not set";
    throw e;
  }

  delete pQuotaStats;
  pQuotaStats = new QuotaStats(config);
}

//------------------------------------------------------------------------------
// Initialize the view
//------------------------------------------------------------------------------
void
HierarchicalView::initialize()
{
  initialize1();
  initialize2();
  initialize3();
}

void
HierarchicalView::initialize1()
{
  pContainerSvc->initialize();

  // Get root container
  try {
    pRoot = pContainerSvc->getContainerMD(1);
  } catch (MDException& e) {
    pRoot = pContainerSvc->createContainer();
    pRoot->setName("/");
    pRoot->setParentId(pRoot->getId());
    updateContainerStore(pRoot.get());
  }
}

//------------------------------------------------------------------------------
// Initialize phase 2
//------------------------------------------------------------------------------
void
HierarchicalView::initialize2()
{
  pFileSvc->initialize();
}

//------------------------------------------------------------------------------
// Initialize phase 3
//------------------------------------------------------------------------------
void
HierarchicalView::initialize3()
{
  //--------------------------------------------------------------------------
  // Scan all the files to reattach them to containers - THIS SHOULD NOT
  // BE DONE! THE INFO NEEDS TO BE STORED WITH CONTAINERS
  //--------------------------------------------------------------------------
  // FileVisitor visitor( pContainerSvc, pQuotaStats, this );
  // pFileSvc->visit( &visitor );
}

//------------------------------------------------------------------------------
// Finalize the view
//------------------------------------------------------------------------------
void
HierarchicalView::finalize()
{
  pContainerSvc->finalize();
  pFileSvc->finalize();
  delete pQuotaStats;
  std::map<std::string, std::string> config;
  pQuotaStats = new QuotaStats(config);
}

//------------------------------------------------------------------------------
// Retrieve a file for given uri
//------------------------------------------------------------------------------
std::shared_ptr<IFileMD>
HierarchicalView::getFile(const std::string& uri, bool follow,
                          size_t* link_depths)
{
  char uriBuffer[uri.length() + 1];
  strcpy(static_cast<char*>(uriBuffer), uri.c_str());
  std::vector<char*> elements;
  size_t lLinkDepths = 0;

  if (uri == "/") {
    MDException e(ENOENT);
    e.getMessage() << " is not a file";
    throw e;
  }

  size_t position;
  eos::PathProcessor::splitPath(elements, static_cast<char*>(uriBuffer));
  std::shared_ptr<IContainerMD> cont =
    findLastContainer(elements, elements.size() - 1, position, link_depths);

  if (position != elements.size() - 1) {
    MDException e(ENOENT);
    e.getMessage() << "Container does not exist";
    throw e;
  }

  std::shared_ptr<IFileMD> file{cont->findFile(elements[position])};

  if (!file) {
    MDException e(ENOENT);
    e.getMessage() << "File " << uri << " does not exist";
    throw e;
  } else {
    if (file->isLink() && follow) {
      if (link_depths == nullptr) {
        link_depths = &lLinkDepths;
      }

      (*link_depths)++;

      if ((*link_depths) > 255) {
        MDException e(ELOOP);
        e.getMessage() << "Too many symbolic links were encountered in "
                       "translating the pathname";
        throw e;
      }

      std::string link = file->getLink();

      if (link[0] != '/') {
        link.insert(0, getUri(cont.get()));
        absPath(link);
      }

      return getFile(link, true, link_depths);
    }
  }

  return file;
}

//------------------------------------------------------------------------------
// Create a file for given uri
//------------------------------------------------------------------------------
std::shared_ptr<IFileMD>
HierarchicalView::createFile(const std::string& uri, uid_t uid, gid_t gid)
{
  // Split the path and find the last container
  char uriBuffer[uri.length() + 1];
  strcpy(static_cast<char*>(uriBuffer), uri.c_str());
  std::vector<char*> elements;
  eos::PathProcessor::splitPath(elements, static_cast<char*>(uriBuffer));
  size_t position;
  std::shared_ptr<IContainerMD> cont =
    findLastContainer(elements, elements.size() - 1, position);

  if (position != elements.size() - 1) {
    MDException e(ENOENT);
    e.getMessage() << "Container does not exist";
    throw e;
  }

  // Check if the file of this name can be inserted
  if (cont->findContainer(elements[position])) {
    MDException e(EEXIST);
    e.getMessage() << "File exist";
    throw e;
  }

  if (cont->findFile(elements[position])) {
    MDException e(EEXIST);
    e.getMessage() << "File exist";
    throw e;
  }

  std::shared_ptr<IFileMD> file{pFileSvc->createFile()};

  if (!file) {
    MDException e(EIO);
    e.getMessage() << "File creation failed";
    throw e;
  }

  file->setName(elements[position]);
  file->setCUid(uid);
  file->setCGid(gid);
  file->setCTimeNow();
  file->setMTimeNow();
  file->clearChecksum(0);
  cont->addFile(file.get());
  updateFileStore(file.get());
  return file;
}

//------------------------------------------------------------------------
//! Create a link for given uri
//------------------------------------------------------------------------
void
HierarchicalView::createLink(const std::string& uri, const std::string& linkuri,
                             uid_t uid, gid_t gid)
{
  std::shared_ptr<IFileMD> file = createFile(uri, uid, gid);

  if (file) {
    file->setLink(linkuri);
    updateFileStore(file.get());
  }
}

//------------------------------------------------------------------------------
// Remove link
//------------------------------------------------------------------------------
void
HierarchicalView::removeLink(const std::string& uri)
{
  return unlinkFile(uri);
}

//------------------------------------------------------------------------------
// Unlink the file for given uri
//------------------------------------------------------------------------------
void
HierarchicalView::unlinkFile(const std::string& uri)
{
  char uriBuffer[uri.length() + 1];
  strcpy(static_cast<char*>(uriBuffer), uri.c_str());
  std::vector<char*> elements;
  eos::PathProcessor::splitPath(elements, static_cast<char*>(uriBuffer));
  size_t position;
  std::shared_ptr<IContainerMD> cont =
    findLastContainer(elements, elements.size() - 1, position);

  if (position != elements.size() - 1) {
    MDException e(ENOENT);
    e.getMessage() << "Container does not exist";
    throw e;
  }

  std::shared_ptr<IFileMD> file{cont->findFile(elements[position])};

  if (!file) {
    MDException e(ENOENT);
    e.getMessage() << "File does not exist";
    throw e;
  }

  file->setContainerId(0);
  file->unlinkAllLocations();
  updateFileStore(file.get());
  cont->removeFile(file->getName());
}

//------------------------------------------------------------------------------
// Unlink the file - this is only used for testing
//------------------------------------------------------------------------------
void
HierarchicalView::unlinkFile(eos::IFileMD* file)
{
  std::shared_ptr<IContainerMD> cont =
    pContainerSvc->getContainerMD(file->getContainerId());
  file->setContainerId(0);
  file->unlinkAllLocations();
  updateFileStore(file);
  cont->removeFile(file->getName());
}

//------------------------------------------------------------------------------
// Remove the file
//------------------------------------------------------------------------------
void
HierarchicalView::removeFile(IFileMD* file)
{
  // Check if the file can be removed
  if (file->getNumLocation() != 0 || file->getNumUnlinkedLocation() != 0) {
    MDException ex(EBADFD);
    ex.getMessage() << "Cannot remove the record. Unlinked replicas ";
    ex.getMessage() << "still exist";
    throw ex;
  }

  if (file->getContainerId() != 0) {
    std::shared_ptr<IContainerMD> cont =
      pContainerSvc->getContainerMD(file->getContainerId());
    cont->removeFile(file->getName());
  }

  pFileSvc->removeFile(file);
}

//------------------------------------------------------------------------------
// Get a container (directory)
//------------------------------------------------------------------------------
std::shared_ptr<IContainerMD>
HierarchicalView::getContainer(const std::string& uri, bool follow,
                               size_t* link_depth)
{
  if (uri == "/") {
    return std::shared_ptr<IContainerMD> {pContainerSvc->getContainerMD(1)};
  }

  size_t lLinkDepth = 0;

  if (link_depth == nullptr) {
    // use local variable in case
    link_depth = &lLinkDepth;
    (*link_depth)++;
  }

  char uriBuffer[uri.length() + 1];
  strcpy(static_cast<char*>(uriBuffer), uri.c_str());
  std::vector<char*> elements;
  eos::PathProcessor::splitPath(elements, static_cast<char*>(uriBuffer));
  size_t position = 0;
  std::shared_ptr<IContainerMD> cont{nullptr};

  if (follow) {
    // Follow all symlinks for all containers
    cont = findLastContainer(elements, elements.size(), position, link_depth);
  } else {
    // Follow all symlinks but not the final container
    cont = findLastContainer(elements, elements.size() - 1, position, link_depth);
    cont = cont->findContainer(elements[elements.size() - 1]);

    if (cont) {
      ++position;
    }
  }

  if (position != (elements.size())) {
    MDException e(ENOENT);
    e.getMessage() << uri << ": No such file or directory";
    throw e;
  }

  return cont;
}

//------------------------------------------------------------------------------
// Create container - method eventually consistent
//------------------------------------------------------------------------------
std::shared_ptr<IContainerMD>
HierarchicalView::createContainer(const std::string& uri, bool createParents)
{
  // Split the path
  if (uri == "/") {
    MDException e(EEXIST);
    e.getMessage() << uri << ": Container exist" << std::endl;
    throw e;
  }

  char uriBuffer[uri.length() + 1];
  strcpy(static_cast<char*>(uriBuffer), uri.c_str());
  std::vector<char*> elements;
  eos::PathProcessor::splitPath(elements, static_cast<char*>(uriBuffer));

  if (elements.empty()) {
    MDException e(EEXIST);
    e.getMessage() << uri << ": File exist" << std::endl;
    throw e;
  }

  // Look for the last existing container
  size_t position;
  std::shared_ptr<IContainerMD> lastContainer =
    findLastContainer(elements, elements.size(), position);

  if (position == elements.size()) {
    MDException e(EEXIST);
    e.getMessage() << uri << ": Container exist" << std::endl;
    throw e;
  }

  // One of the parent containers does not exist
  if ((!createParents) && (position < elements.size() - 1)) {
    MDException e(ENOENT);
    e.getMessage() << uri << ": Parent does not exist" << std::endl;
    throw e;
  }

  if (lastContainer->findFile(elements[position])) {
    MDException e(EEXIST);
    e.getMessage() << "File exists" << std::endl;
    throw e;
  }

  // Create the container with all missing parents if required. If a crash
  // happens during the addContainer call and the updateContainerStore then
  // we curate the list of subcontainers in the ContainerMD::findContainer
  // method.
  for (size_t i = position; i < elements.size(); ++i) {
    std::shared_ptr<IContainerMD> newContainer{
      pContainerSvc->createContainer()};
    newContainer->setName(elements[i]);
    newContainer->setCTimeNow();
    lastContainer->addContainer(newContainer.get());
    lastContainer.swap(newContainer);
    updateContainerStore(lastContainer.get());
  }

  return lastContainer;
}

//------------------------------------------------------------------------------
// Remove container
//------------------------------------------------------------------------------
void
HierarchicalView::removeContainer(const std::string& uri, bool recursive)
{
  // Find the container
  if (uri == "/") {
    MDException e(EPERM);
    e.getMessage() << "Permission denied.";
    throw e;
  }

  char uriBuffer[uri.length() + 1];
  strcpy(static_cast<char*>(uriBuffer), uri.c_str());
  std::vector<char*> elements;
  eos::PathProcessor::splitPath(elements, static_cast<char*>(uriBuffer));
  size_t position;
  auto parent = findLastContainer(elements, elements.size() - 1, position);

  if ((position != (elements.size() - 1))) {
    MDException e(ENOENT);
    e.getMessage() << uri << ": No such file or directory";
    throw e;
  }

  // Check if the container exist and remove it
  auto cont = parent->findContainer(elements[elements.size() - 1]);

  if (!cont) {
    MDException e(ENOENT);
    e.getMessage() << uri << ": No such file or directory";
    throw e;
  }

  if ((cont->getNumContainers() != 0 || cont->getNumFiles() != 0) &&
      !recursive) {
    MDException e(ENOTEMPTY);
    e.getMessage() << uri << ": Container is not empty";
    throw e;
  }

  if (recursive) {
    cleanUpContainer(cont.get());
  }

  // This is a two-step delete
  pContainerSvc->removeContainer(cont.get());
  parent->removeContainer(cont->getName());
}

//------------------------------------------------------------------------------
// Find the last existing container in the path
//------------------------------------------------------------------------------
std::shared_ptr<IContainerMD>
HierarchicalView::findLastContainer(std::vector<char*>& elements, size_t end,
                                    size_t& index, size_t* link_depths)
{
  std::shared_ptr<IContainerMD> current = pRoot;
  std::shared_ptr<IContainerMD> found;
  size_t position = 0;

  while (position < end) {
    found = current->findContainer(elements[position]);

    if (!found) {
      // Check if link
      std::shared_ptr<IFileMD> flink = current->findFile(elements[position]);

      if (flink) {
        if (flink->isLink()) {
          if (link_depths != nullptr) {
            (*link_depths)++;

            if ((*link_depths) > 255) {
              MDException e(ELOOP);
              e.getMessage() << "Too many symbolic links were encountered "
                             "in translating the pathname";
              throw e;
            }
          }

          std::string link = flink->getLink();

          if (link[0] != '/') {
            link.insert(0, getUri(current.get()));
            absPath(link);
          }

          found = getContainer(link, false, link_depths);

          if (!found) {
            index = position;
            return current;
          }
        }
      }

      if (!found) {
        index = position;
        return current;
      }
    }

    current = found;
    ++position;
  }

  index = position;
  return current;
}

//------------------------------------------------------------------------------
// Clean up the container's children
//------------------------------------------------------------------------------
void
HierarchicalView::cleanUpContainer(IContainerMD* cont)
{
  (void)cont->cleanUp();
}

//------------------------------------------------------------------------------
// Get uri for the container
//------------------------------------------------------------------------------
std::string
HierarchicalView::getUri(const IContainerMD* container) const
{
  // Check the input
  if (container == nullptr) {
    MDException ex;
    ex.getMessage() << "Invalid container (zero pointer)";
    throw ex;
  }

  // Gather the uri elements
  std::vector<std::string> elements;
  elements.reserve(10);
  std::shared_ptr<IContainerMD> cursor =
    pContainerSvc->getContainerMD(container->getId());

  while (cursor->getId() != 1) {
    elements.push_back(cursor->getName());
    cursor = pContainerSvc->getContainerMD(cursor->getParentId());
  }

  // Assemble the uri
  std::string path = "/";
  std::vector<std::string>::reverse_iterator rit;

  for (rit = elements.rbegin(); rit != elements.rend(); ++rit) {
    path += *rit;
    path += "/";
  }

  return path;
}

//------------------------------------------------------------------------------
// Get uri for the file
//------------------------------------------------------------------------------
std::string
HierarchicalView::getUri(const IFileMD* file) const
{
  // Check the input
  if (file == nullptr) {
    MDException ex;
    ex.getMessage() << "Invalid file (zero pointer)";
    throw ex;
  }

  // Get the uri
  std::shared_ptr<IContainerMD> cont =
    pContainerSvc->getContainerMD(file->getContainerId());
  std::string path = getUri(cont.get());
  return path + file->getName();
}

//------------------------------------------------------------------------
// Get real path translating existing symlink
//------------------------------------------------------------------------
std::string HierarchicalView::getRealPath(const std::string& uri)
{
  size_t link_depths = 0;
  char uriBuffer[uri.length() + 1];
  strcpy(uriBuffer, uri.c_str());
  std::vector<char*> elements;

  if (uri == "/") {
    MDException e(ENOENT);
    e.getMessage() << " is not a file";
    throw e;
  }

  eos::PathProcessor::splitPath(elements, uriBuffer);
  size_t position;
  auto cont = findLastContainer(elements, elements.size() - 1, position,
                                &link_depths);

  if (position != elements.size() - 1) {
    MDException e(ENOENT);
    e.getMessage() << "Container does not exist";
    throw e;
  }

  // replace the last existing container with the resolved container path
  std::string newcontainer = getUri(cont.get());
  size_t oldlength = 0;

  for (size_t i = 0; i < position; i++) {
    oldlength += strlen(elements[i]) + 1;
  }

  std::string newpath = uri;
  newpath.erase(0, oldlength + 1);
  newpath.insert(0, newcontainer);
  return newpath;
}

//------------------------------------------------------------------------------
// Get quota node id concerning given container
//------------------------------------------------------------------------------
IQuotaNode*
HierarchicalView::getQuotaNode(const IContainerMD* container, bool search)
{
  // Initial sanity check
  if (container == nullptr) {
    MDException ex;
    ex.getMessage() << "Invalid container (zero pointer)";
    throw ex;
  }

  if (pQuotaStats == nullptr) {
    MDException ex;
    ex.getMessage() << "No QuotaStats placeholder registered";
    throw ex;
  }

  // Search for the node
  std::shared_ptr<IContainerMD> current =
    pContainerSvc->getContainerMD(container->getId());

  if (search) {
    while (current->getName() != pRoot->getName() &&
           (current->getFlags() & QUOTA_NODE_FLAG) == 0) {
      current = pContainerSvc->getContainerMD(current->getParentId());
    }
  }

  // We have either found a quota node or reached root without finding one
  // so we need to double check whether the current container has an
  // associated quota node
  if ((current->getFlags() & QUOTA_NODE_FLAG) == 0) {
    return nullptr;
  }

  IQuotaNode* node = pQuotaStats->getQuotaNode(current->getId());

  if (node != nullptr) {
    return node;
  }

  return pQuotaStats->registerNewNode(current->getId());
}

//------------------------------------------------------------------------------
// Register the container to be a quota node
//------------------------------------------------------------------------------
IQuotaNode*
HierarchicalView::registerQuotaNode(IContainerMD* container)
{
  // Initial sanity check
  if (container == nullptr) {
    MDException ex;
    ex.getMessage() << "Invalid container (zero pointer)";
    throw ex;
  }

  if (pQuotaStats == nullptr) {
    MDException ex;
    ex.getMessage() << "No QuotaStats placeholder registered";
    throw ex;
  }

  if ((container->getFlags() & QUOTA_NODE_FLAG) != 0) {
    MDException ex;
    ex.getMessage() << "Already a quota node: " << container->getId();
    throw ex;
  }

  IQuotaNode* node = pQuotaStats->registerNewNode(container->getId());
  container->getFlags() |= QUOTA_NODE_FLAG;
  updateContainerStore(container);
  return node;
}

//------------------------------------------------------------------------------
// Remove the quota node
//------------------------------------------------------------------------------
void
HierarchicalView::removeQuotaNode(IContainerMD* container)
{
  // Sanity checks
  if (container == nullptr) {
    MDException ex;
    ex.getMessage() << "Invalid container (zero pointer)";
    throw ex;
  }

  if (pQuotaStats == nullptr) {
    MDException ex;
    ex.getMessage() << "No QuotaStats placeholder registered";
    throw ex;
  }

  if ((container->getFlags() & QUOTA_NODE_FLAG) == 0) {
    MDException ex;
    ex.getMessage() << "Not a quota node: " << container->getId();
    throw ex;
  }

  // Get the quota node and meld it with the parent node if present
  IQuotaNode* node = getQuotaNode(container);
  IQuotaNode* parent = nullptr;

  if (container->getId() != 1) {
    parent = getQuotaNode(
               pContainerSvc->getContainerMD(container->getParentId()).get(), true);
  }

  container->getFlags() &= ~QUOTA_NODE_FLAG;
  updateContainerStore(container);

  if (parent != nullptr) {
    try {
      parent->meld(node);
    } catch (const std::runtime_error& e) {
      MDException ex;
      ex.getMessage() << "Failed quota node meld: " << e.what();
      throw ex;
    }
  }

  pQuotaStats->removeNode(container->getId());
}

//------------------------------------------------------------------------------
// Rename container
//------------------------------------------------------------------------------
void
HierarchicalView::renameContainer(IContainerMD* container,
                                  const std::string& newName)
{
  if (container == nullptr) {
    MDException ex;
    ex.getMessage() << "Invalid container (zero pointer)";
    throw ex;
  }

  if (newName.empty()) {
    MDException ex;
    ex.getMessage() << "Invalid new name (empty)";
    throw ex;
  }

  if (newName.find('/') != std::string::npos) {
    MDException ex;
    ex.getMessage() << "Name cannot contain slashes: " << newName;
    throw ex;
  }

  if (container->getId() == container->getParentId()) {
    MDException ex;
    ex.getMessage() << "Cannot rename /";
    throw ex;
  }

  std::shared_ptr<IContainerMD> parent{
    pContainerSvc->getContainerMD(container->getParentId())};

  if (parent->findContainer(newName) != nullptr) {
    MDException ex;
    ex.getMessage() << "Container exists: " << newName;
    throw ex;
  }

  if (parent->findFile(newName) != nullptr) {
    MDException ex;
    ex.getMessage() << "File exists: " << newName;
    throw ex;
  }

  parent->removeContainer(container->getName());
  container->setName(newName);
  parent->addContainer(container);
  updateContainerStore(container);
}

//------------------------------------------------------------------------------
// Rename file
//------------------------------------------------------------------------------
void
HierarchicalView::renameFile(IFileMD* file, const std::string& newName)
{
  if (file == nullptr) {
    MDException ex;
    ex.getMessage() << "Invalid file (zero pointer)";
    throw ex;
  }

  if (newName.empty()) {
    MDException ex;
    ex.getMessage() << "Invalid new name (empty)";
    throw ex;
  }

  if (newName.find('/') != std::string::npos) {
    MDException ex;
    ex.getMessage() << "Name cannot contain slashes: " << newName;
    throw ex;
  }

  std::shared_ptr<IContainerMD> parent{
    pContainerSvc->getContainerMD(file->getContainerId())};

  if (parent->findContainer(newName) != nullptr) {
    MDException ex;
    ex.getMessage() << "Container exists: " << newName;
    throw ex;
  }

  if (parent->findFile(newName) != nullptr) {
    MDException ex;
    ex.getMessage() << "File exists: " << newName;
    throw ex;
  }

  parent->removeFile(file->getName());
  file->setName(newName);
  parent->addFile(file);
  updateFileStore(file);
}

//------------------------------------------------------------------------------
// Abspath sanitizing all '..' and '.' in a path
//------------------------------------------------------------------------------
void
HierarchicalView::absPath(std::string& mypath)
{
  std::string path = mypath;
  std::string abspath;
  size_t rpos = 4096;

  while ((rpos = path.rfind('/', rpos)) != std::string::npos) {
    rpos--;
    std::string tp = path.substr(rpos + 1);
    path.erase(rpos + 1);

    if (tp == "/") {
      continue;
    }

    if (tp == "/.") {
      continue;
    }

    if (tp == "/..") {
      rpos = path.rfind('/', rpos);
      path.erase(rpos);
      rpos--;
      continue;
    }

    abspath.insert(0, tp);

    if (rpos <= 0) {
      break;
    }
  }

  mypath = abspath;
}

EOSNSNAMESPACE_END
