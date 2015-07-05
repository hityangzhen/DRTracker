#ifndef __CORE_STATIC_INFO
#define __CORE_STATIC_INFO

/**
 * Static properties of an image.
 */

#include "core/basictypes.h"
#include <tr1/unordered_map>
#include <map>
#include <iostream>
#include "core/sync.h"

#include "core/static_info.pb.h" //protobuf head file

class Inst;
class StaticInfo;

typedef uint32 image_t;
typedef uint32 inst_t;
typedef uint32 opcode_t;

#define INVALID_IMAGE_ID static_cast<image_t>(-1)
#define PSEUDO_IMAGE_NAME  "PSEUDO_IMAGE"


//An image can be a main executable or a library image
class Image {
public:
	Inst *Find(address_t offset);
	bool IsCommonLib();
	bool IsLibc();
	bool IsPthread();

	std::string ShortName();
	std::string ToString() {return ShortName();}

	image_t id() { return proto_->id(); }
	const std::string &name() { return proto_->name(); }

private:
	typedef std::tr1::unordered_map<address_t,Inst *> InstAddrMap;

	Image(ImageProto *proto) : proto_(proto) {}
	~Image(){}

	void Register(Inst *inst);
	InstAddrMap inst_offset_map_; //store static instructions for the image
	ImageProto *proto_;

private:
	friend class StaticInfo;
	DISALLOW_COPY_CONSTRUCTORS(Image);
};

//An instruction in an image
class Inst {
public:
	bool HasOpcode() {return proto_->has_opcode();}
	bool HasDebugInfo() { return proto_->has_debug_info(); }
	void SetOpcode(opcode_t c) {proto_->set_opcode(c);}
	void SetDebugInfo(const std::string &fileName, int line, int column);
	std::string ToString();

	inst_t id() {return proto_->id();}
	Image *image() {return image_;}
	address_t offset() {return proto_->offset();}
	opcode_t opcode() {return proto_->opcode();}
	std::string DebugInfoStr();

	std::string GetFileName() { return proto_->debug_info().file_name(); }
	int GetLine() { return proto_->debug_info().line(); }
private:

	Inst(Image *image,InstProto *proto):image_(image),proto_(proto) {}
	~Inst() {}
	Image * image_;
	InstProto *proto_;
	
private:
	friend class StaticInfo;
	DISALLOW_COPY_CONSTRUCTORS(Inst);
};

//The static info for all executables and library images
class StaticInfo {
public:
	explicit StaticInfo(Mutex *lock);
	~StaticInfo() {}

	Image *CreateImage(const std::string &name);
	Inst *CreateInst(Image *image,address_t offset);
	Image *FindImage(const std::string &name);
	Image *FindImage(image_t id);
	Inst *FindInst(inst_t  id);

	//protobuf serialization
	void Load(const std::string &db_name);
	void Save(const std::string &db_name);

private:
	typedef std::map<image_t,Image *> ImageMap;
	typedef std::tr1::unordered_map<inst_t,Inst *> InstMap;

	image_t GetNextImageID() {return ++curr_image_id_;}
	inst_t GetNextInstID() {return ++curr_inst_id_;}

	Mutex *lock_;
	image_t curr_image_id_;
	inst_t curr_inst_id_;
	ImageMap image_map_;
	InstMap inst_map_;

	StaticInfoProto proto_;

private:
	DISALLOW_COPY_CONSTRUCTORS(StaticInfo);
};

#endif /* __CORE_STATIC_INFO */