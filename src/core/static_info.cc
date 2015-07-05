#include "core/static_info.h"
#include <fstream>
#include <sstream>

Inst *Image::Find(address_t offset)
{
	InstAddrMap::iterator found=inst_offset_map_.find(offset);
	if(found==inst_offset_map_.end())
		return NULL;
	else
		return found->second;
}

bool Image::IsCommonLib()
{
	using std::string;
	const string &imageName=name();
	if(imageName.find("libc") != string::npos ||
	   imageName.find("libpthread") != string::npos ||
	   imageName.find("ld-") != string::npos ||
	   imageName.find("libstdc++") != string::npos ||
	   imageName.find("libgcc_s") != string::npos ||
	   imageName.find("libm") != string::npos ||
       imageName.find("libnsl") != string::npos ||
       imageName.find("librt") != string::npos ||
       imageName.find("libdl") != string::npos ||
       imageName.find("libz") != string::npos ||
       imageName.find("libcrypt") != string::npos ||
       imageName.find("libdb") != string::npos ||
       imageName.find("libexpat") != string::npos ||
       imageName.find("libbz2") != string::npos )
		return true;
	else
		return false;
}

bool Image::IsLibc()
{
	if(name().find("libc") != std::string::npos)
		return true;
	return false;
}

bool Image::IsPthread()
{
	if(name().find("libpthread") != std::string::npos)
		return true;
	return false;	
}

std::string Image::ShortName()
{
	size_t found=name().find_last_of('/');
	if(found!=std::string::npos)
		return name().substr(found+1);
	else
		return name();
}

void Image::Register(Inst *inst)
{
	inst_offset_map_[inst->offset()]=inst;
}

void Inst::SetDebugInfo(const std::string &fileName, int line, int column)
{
  DebugInfoProto *diProto = proto_->mutable_debug_info();
  diProto->set_file_name(fileName);
  diProto->set_line(line);
  diProto->set_column(column);
}

std::string Inst::DebugInfoStr()
{
	const DebugInfoProto &diProto=proto_->debug_info();
	size_t found = diProto.file_name().find_last_of('/');
    std::stringstream ss;
    if (found != std::string::npos)
      ss << diProto.file_name().substr(found + 1);
    else
      ss << diProto.file_name();
  	ss << " +" << std::dec << diProto.line();
  	return ss.str();
}

std::string Inst::ToString()
{
	std::stringstream ss;
	ss << std::hex << id() << " " << image_->ToString() << " 0x" << offset();
  	ss << " (" << DebugInfoStr() << ")";
  	return ss.str();
}


StaticInfo::StaticInfo(Mutex *lock)
	:lock_(lock),curr_image_id_(0),curr_inst_id_(0)
{}

Image *StaticInfo::CreateImage(const std::string &name)
{
	ImageProto *imageProto=proto_.add_image();//for protobuf repeated
	image_t imageId=GetNextImageID();
	imageProto->set_id(imageId);
	imageProto->set_name(name);
	Image *image=new Image(imageProto);
	image_map_[imageId]=image;
	return image;
}

Inst *StaticInfo::CreateInst(Image *image,address_t offset)
{
	InstProto *instProto=proto_.add_inst();
	inst_t instId=GetNextInstID();
	instProto->set_id(instId);
	instProto->set_image_id(image->id());
	instProto->set_offset(offset);
	Inst *inst=new Inst(image,instProto);
	inst_map_[instId]=inst;
	image->Register(inst);
	return inst;
}

Image *StaticInfo::FindImage(const std::string &name)
{
  for (ImageMap::iterator it = image_map_.begin();
       it != image_map_.end(); ++it) {
    Image *image = it->second;
    if (image->name().compare(name) == 0)
      return image;
  }
  return NULL;
}

Image *StaticInfo::FindImage(image_t id)
{
  ImageMap::iterator it = image_map_.find(id);
  if (it == image_map_.end())
    return NULL;
  else
    return it->second;
}

Inst *StaticInfo::FindInst(inst_t id) {
  InstMap::iterator it = inst_map_.find(id);
  if (it == inst_map_.end())
    return NULL;
  else
    return it->second;
}


void StaticInfo::Load(const std::string &db_name)
{
	std::ifstream in(db_name.c_str(),std::ios::in | std::ios::binary);
	proto_.ParseFromIstream(&in);
	in.close();

	//setup image map
	for(int i=0;i<proto_.image_size();i++) {
		ImageProto *imageProto=proto_.mutable_image(i);
		Image *image=new  Image(imageProto);
		image_t imageId=image->id();
		image_map_[imageId]=image;

		if(imageId>curr_image_id_)
			curr_image_id_=imageId;
	}

	//setup Inst map
	for(int i=0;i<proto_.inst_size();i++) {
		InstProto *instProto=proto_.mutable_inst(i);
		Image *image=FindImage(instProto->image_id());
		Inst *inst=new Inst(image,instProto);
		inst_t instId=inst->id();
		inst_map_[instId]=inst;

		image->Register(inst);
		if(instId>curr_inst_id_)
			curr_inst_id_=instId;
	}
}

void StaticInfo::Save(const std::string &db_name)
{
	std::ofstream out(db_name.c_str(),std::ios::out | std::ios::trunc | std::ios::binary);
	proto_.SerializeToOstream(&out);
	out.close();
}

