

#ifndef _PLATFORM_PortAndExprTreeNode_H
#define _PLATFORM_PortAndExprTreeNode_H

#include <string>
#include <vector>
#include <memory>

namespace platform
{
/// Operation name
typedef int OperType;

// Wild card for matching operation names
OperType const AnyOperation = -1;

/**
 * CPort describes the port interface on a hardware core
 */
struct CPort
{
public:
    /// CPort direction type
    enum DirType
    {
        CPortIn, /// Input port
        CPortOut, /// Output port
        CPortInOut, /// Inout port
        CPortEnd /// End
    };

    /// Constructor.
    CPort(std::string name = "", DirType dir = CPortInOut,
          unsigned bit_width = 1, std::string constant = "");
    /// Copy constructor.
    CPort(const CPort& other);

    /// Destructor.
    virtual ~CPort() { }

    /// Get port name.
    /// @return Returns port name in string.
    std::string getName() const { return mName; }
    /// Set port name.
    /// @param name New port name in string.
    void setName(std::string name) { mName = name; }

    /// Get port direction.
    /// @return Returns port direction.
    DirType getDirection() const { return mDir; }
    /// Set port direction.
    /// @param dir New port direction.
    void setDirection(DirType dir) { mDir = dir; }

    /// Get the constant string.
    /// Returns emtpy string if this port is with a variable.
    std::string getConstString() const { return mConstString; }
    /// Set the constant string.
    void setConstString(std::string constant) { mConstString = constant; }

    /// Get the biwidth of the port.
    unsigned getBitWidth() const { return mBitWidth; }
    /// Set the biwidth of the port.
    void setBitWidth(unsigned bit_width)
    {
        // clear const string first we are setting a var bitwidth
        mConstString = "";
        mBitWidth = bit_width;
    }

    /// Convert string to port direction.
    static DirType string2DirType(std::string dir_str);

public:
    /// Port name
    std::string mName;
    /// Port direction
    DirType mDir;
    /// Port bitwidth
    unsigned mBitWidth;
    /// Constant string
    std::string mConstString;
};

/// Functional unit port struct
struct FuPort : public CPort
{
  // TODO: Add FU-specific members and APIs.
  // e.g., control(opcode)/data operands, etc.
public:
    /// Constructor.
    FuPort(std::string name = "",
           DirType dir = CPortInOut,
           unsigned bit_width = 1) :
        CPort(name, dir, bit_width) { }
    /// Copy constructor.
    FuPort(const FuPort& other) : CPort(other) { }

    /// Destructor.
    virtual ~FuPort() { }
};

/// Memory port struct
struct MemPort : public CPort
{
  // TODO: Add memory-specific members and APIs.
  // e.g., synchronous/acynchronous, data/address, etc.
public:
    /// Constructor.
    MemPort(std::string name = "",
            DirType dir = CPortInOut,
            unsigned bit_width = 0) :
        CPort(name, dir, bit_width) { }
    /// Copy constructor.
    MemPort(const MemPort& other) : CPort(other) { }
    /// Destructor.
    virtual ~MemPort() { }

    /// Convert string to port direction.
    static DirType string2DirType(std::string dir_str);
    /// Convert port direction to string.
};

/// Map: Port name --> CPort*
typedef std::vector<CPort> CPortList;
/// CPort iterator.
typedef CPortList::iterator CPortIter;

// internal nodes, must be binary operators
struct ExprTreeNode
{
    /// Name
    std::string mName;
    /// Opcode
    OperType mOpcode;
    /// Sub trees.
    std::shared_ptr<ExprTreeNode> mLHS;
    std::shared_ptr<ExprTreeNode> mRHS;

    ExprTreeNode(std::string name = "", OperType opc = AnyOperation,
                 std::shared_ptr<ExprTreeNode> l = 0,
                 std::shared_ptr<ExprTreeNode> r = 0)
        : mName(name), mOpcode(opc), mLHS(l), mRHS(r) { }


    virtual ~ExprTreeNode() { };
    OperType getOpcode() const { return mOpcode; }
    std::string getName() const { return mName; }

    std::shared_ptr<const ExprTreeNode> getLHS() const { return mLHS; }
    std::shared_ptr<const ExprTreeNode> getRHS() const { return mRHS; }

    bool isLeaf() const { return (getLHS() == 0) && (getRHS() == 0); }
    bool isBinaryOp() const { return (getLHS() != 0) && (getRHS() != 0); }
    std::string toString() const;
};

typedef std::shared_ptr<ExprTreeNode> ExprTreeNode_shared_ptr;
typedef std::shared_ptr<const ExprTreeNode> ExprTreeNode_const_shared_ptr;

} //< namespace platform
#endif

