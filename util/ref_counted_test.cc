#include "iomgr/ref_counted.h"

#include <arpa/inet.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

namespace iomgr {

class SelfAssign : public RefCounted<SelfAssign> {
 protected:
  friend class RefCounted<SelfAssign>;

  virtual ~SelfAssign() = default;
};

class Derived : public SelfAssign {
 protected:
  friend class RefCounted<Derived>;

  ~Derived() = default;
};

class CheckDerivedMemberAccess : public RefPtr<SelfAssign> {
 public:
  CheckDerivedMemberAccess() {
    SelfAssign** pptr = &ptr_;
    EXPECT_EQ(*pptr, ptr_);
  }
};

class RefCountedPtrToSelf : public RefCounted<RefCountedPtrToSelf> {
 public:
  RefCountedPtrToSelf() : self_ptr_(this) {}
  static bool was_destroyed() { return was_destroyed_; }
  static void reset_was_destroyed() { was_destroyed_ = false; }

  RefPtr<RefCountedPtrToSelf> self_ptr_;

 private:
  friend class RefCounted<RefCountedPtrToSelf>;

  ~RefCountedPtrToSelf() { was_destroyed_ = true; }

  static bool was_destroyed_;
};

bool RefCountedPtrToSelf::was_destroyed_ = false;

class RefCountedBase : public RefCounted<RefCountedBase> {
 public:
  RefCountedBase() { ++constructor_count_; }
  static int constructor_count() { return constructor_count_; }
  static int destructor_count() { return destructor_count_; }
  static void reset_count() {
    constructor_count_ = 0;
    destructor_count_ = 0;
  }

 protected:
  virtual ~RefCountedBase() { ++destructor_count_; }

 private:
  friend class RefCounted<RefCountedBase>;

  static int constructor_count_;
  static int destructor_count_;
};

int RefCountedBase::constructor_count_;
int RefCountedBase::destructor_count_;

class RefCountedDerived : public RefCountedBase {
 public:
  RefCountedDerived() { ++constructor_count_; }
  static int constructor_count() { return constructor_count_; }
  static int destructor_count() { return destructor_count_; }
  static void reset_count() {
    constructor_count_ = 0;
    destructor_count_ = 0;
  }

 protected:
  ~RefCountedDerived() { ++destructor_count_; }

 private:
  friend class RefCounted<RefCountedDerived>;

  static int constructor_count_;
  static int destructor_count_;
};

int RefCountedDerived::constructor_count_;
int RefCountedDerived::destructor_count_;

class Other : public RefCounted<Other> {
 private:
  friend class RefCounted<Other>;

  ~Other() = default;
};

class CheckRefCountedNULL : public RefCounted<CheckRefCountedNULL> {
 public:
  void set_auto_refptr(RefPtr<CheckRefCountedNULL>* ptr) { ptr_ = ptr; }

 protected:
  ~CheckRefCountedNULL() {
    EXPECT_NE(ptr_, nullptr);
    EXPECT_EQ(ptr_->get(), nullptr);
  }

 private:
  friend class RefCounted<CheckRefCountedNULL>;

  RefPtr<CheckRefCountedNULL>* ptr_{nullptr};
};

TEST(RefCountedTest, TestSelfAssignment) {
  SelfAssign* p = new SelfAssign;
  RefPtr<SelfAssign> var(p);
  var = *&var;
  EXPECT_EQ(var.get(), p);
  var = std::move(var);
  EXPECT_EQ(var.get(), p);
  var.swap(var);
  EXPECT_EQ(var.get(), p);
}

TEST(RefCountedTest, RefCountedMemberAccess) { CheckDerivedMemberAccess check; }

TEST(RefCountedTest, RefCountedPtrToSelfAssignment) {
  RefCountedPtrToSelf::reset_was_destroyed();

  RefCountedPtrToSelf* check = new RefCountedPtrToSelf();
  EXPECT_FALSE(RefCountedPtrToSelf::was_destroyed());
  check->self_ptr_ = nullptr;
  EXPECT_TRUE(RefCountedPtrToSelf::was_destroyed());
}

TEST(RefCountedTest, BooleanTesting) {
  RefPtr<SelfAssign> var = new SelfAssign();
  EXPECT_TRUE(var);
  EXPECT_FALSE(!var);

  RefPtr<SelfAssign> null;
  EXPECT_FALSE(null);
  EXPECT_TRUE(!null);
}

TEST(RefCountedTest, MoveAssignment) {
  RefCountedBase::reset_count();

  {
    RefCountedBase* raw = new RefCountedBase;
    RefPtr<RefCountedBase> p1(raw);
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(0, RefCountedBase::destructor_count());

    {
      RefPtr<RefCountedBase> p2;

      p2 = std::move(p1);
      EXPECT_EQ(1, RefCountedBase::constructor_count());
      EXPECT_EQ(0, RefCountedBase::destructor_count());
      EXPECT_EQ(nullptr, p1.get());
      EXPECT_EQ(raw, p2.get());
    }
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(1, RefCountedBase::destructor_count());
  }
  EXPECT_EQ(1, RefCountedBase::constructor_count());
  EXPECT_EQ(1, RefCountedBase::destructor_count());
}

TEST(RefCountedTest, MoveAssignmentSameInstance1) {
  RefCountedBase::reset_count();

  {
    RefCountedBase* raw = new RefCountedBase;
    RefPtr<RefCountedBase> p1(raw);
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(0, RefCountedBase::destructor_count());

    {
      RefPtr<RefCountedBase> p2(p1);
      EXPECT_EQ(1, RefCountedBase::constructor_count());
      EXPECT_EQ(0, RefCountedBase::destructor_count());

      p1 = std::move(p2);
      EXPECT_EQ(1, RefCountedBase::constructor_count());
      EXPECT_EQ(0, RefCountedBase::destructor_count());
      EXPECT_EQ(raw, p1.get());
      EXPECT_EQ(nullptr, p2.get());
    }
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(0, RefCountedBase::destructor_count());
  }
  EXPECT_EQ(1, RefCountedBase::constructor_count());
  EXPECT_EQ(1, RefCountedBase::destructor_count());
}

TEST(RefCountedTest, MoveAssignmentSameInstance2) {
  RefCountedBase::reset_count();

  {
    RefCountedBase* raw = new RefCountedBase;
    RefPtr<RefCountedBase> p1(raw);
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(0, RefCountedBase::destructor_count());

    {
      RefPtr<RefCountedBase> p2(p1);
      EXPECT_EQ(1, RefCountedBase::constructor_count());
      EXPECT_EQ(0, RefCountedBase::destructor_count());

      p2 = std::move(p1);
      EXPECT_EQ(1, RefCountedBase::constructor_count());
      EXPECT_EQ(0, RefCountedBase::destructor_count());
      EXPECT_EQ(nullptr, p1.get());
      EXPECT_EQ(raw, p2.get());
    }
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(1, RefCountedBase::destructor_count());
  }
  EXPECT_EQ(1, RefCountedBase::constructor_count());
  EXPECT_EQ(1, RefCountedBase::destructor_count());
}

TEST(RefCountedTest, MoveAssignmentDifferentInstance) {
  RefCountedBase::reset_count();

  {
    RefCountedBase* raw1 = new RefCountedBase;
    RefPtr<RefCountedBase> p1(raw1);
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(0, RefCountedBase::destructor_count());

    {
      RefCountedBase* raw2 = new RefCountedBase;
      RefPtr<RefCountedBase> p2(raw2);
      EXPECT_EQ(2, RefCountedBase::constructor_count());
      EXPECT_EQ(0, RefCountedBase::destructor_count());

      p1 = std::move(p2);
      EXPECT_EQ(2, RefCountedBase::constructor_count());
      EXPECT_EQ(1, RefCountedBase::destructor_count());
      EXPECT_EQ(raw2, p1.get());
      EXPECT_EQ(nullptr, p2.get());
    }
    EXPECT_EQ(2, RefCountedBase::constructor_count());
    EXPECT_EQ(1, RefCountedBase::destructor_count());
  }
  EXPECT_EQ(2, RefCountedBase::constructor_count());
  EXPECT_EQ(2, RefCountedBase::destructor_count());
}

TEST(RefCountedTest, MoveAssignmentSelfMove) {
  RefCountedBase::reset_count();

  {
    RefCountedBase* raw1 = new RefCountedBase;
    RefPtr<RefCountedBase> p1(raw1);
    RefPtr<RefCountedBase>& p1_ref = p1;

    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(0, RefCountedBase::destructor_count());

    p1 = std::move(p1_ref);
  }
  EXPECT_EQ(1, RefCountedBase::constructor_count());
  EXPECT_EQ(1, RefCountedBase::destructor_count());
}

TEST(RefCountedTest, MoveAssignmentDerived) {
  RefCountedBase::reset_count();
  RefCountedDerived::reset_count();

  {
    RefCountedBase* raw1 = new RefCountedBase();
    RefPtr<RefCountedBase> p1(raw1);
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(0, RefCountedBase::destructor_count());
    EXPECT_EQ(0, RefCountedDerived::constructor_count());
    EXPECT_EQ(0, RefCountedDerived::destructor_count());

    {
      RefCountedDerived* raw2 = new RefCountedDerived();
      RefPtr<RefCountedDerived> p2(raw2);
      EXPECT_EQ(2, RefCountedBase::constructor_count());
      EXPECT_EQ(0, RefCountedBase::destructor_count());
      EXPECT_EQ(1, RefCountedDerived::constructor_count());
      EXPECT_EQ(0, RefCountedDerived::destructor_count());

      p1 = std::move(p2);
      EXPECT_EQ(2, RefCountedBase::constructor_count());
      EXPECT_EQ(1, RefCountedBase::destructor_count());
      EXPECT_EQ(1, RefCountedDerived::constructor_count());
      EXPECT_EQ(0, RefCountedDerived::destructor_count());
      EXPECT_EQ(raw2, p1.get());
      EXPECT_EQ(nullptr, p2.get());
    }
    EXPECT_EQ(2, RefCountedBase::constructor_count());
    EXPECT_EQ(1, RefCountedBase::destructor_count());
    EXPECT_EQ(1, RefCountedDerived::constructor_count());
    EXPECT_EQ(0, RefCountedDerived::destructor_count());
  }
  EXPECT_EQ(2, RefCountedBase::constructor_count());
  EXPECT_EQ(2, RefCountedBase::destructor_count());
  EXPECT_EQ(1, RefCountedDerived::constructor_count());
  EXPECT_EQ(1, RefCountedDerived::destructor_count());
}

TEST(RefCountedTest, MoveConstructor) {
  RefCountedBase::reset_count();

  {
    RefCountedBase* raw = new RefCountedBase();
    RefPtr<RefCountedBase> p1(raw);
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(0, RefCountedBase::destructor_count());

    {
      RefPtr<RefCountedBase> p2(std::move(p1));
      EXPECT_EQ(1, RefCountedBase::constructor_count());
      EXPECT_EQ(0, RefCountedBase::destructor_count());
      EXPECT_EQ(nullptr, p1.get());
      EXPECT_EQ(raw, p2.get());
    }
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(1, RefCountedBase::destructor_count());
  }
  EXPECT_EQ(1, RefCountedBase::constructor_count());
  EXPECT_EQ(1, RefCountedBase::destructor_count());
}

TEST(RefCountedTest, MoveConstructorDerived) {
  RefCountedBase::reset_count();
  RefCountedDerived::reset_count();

  {
    RefCountedDerived* raw1 = new RefCountedDerived();
    RefPtr<RefCountedDerived> p1(raw1);
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(0, RefCountedBase::destructor_count());
    EXPECT_EQ(1, RefCountedDerived::constructor_count());
    EXPECT_EQ(0, RefCountedDerived::destructor_count());

    {
      RefPtr<RefCountedBase> p2(std::move(p1));
      EXPECT_EQ(1, RefCountedBase::constructor_count());
      EXPECT_EQ(0, RefCountedBase::destructor_count());
      EXPECT_EQ(1, RefCountedDerived::constructor_count());
      EXPECT_EQ(0, RefCountedDerived::destructor_count());
      EXPECT_EQ(nullptr, p1.get());
      EXPECT_EQ(raw1, p2.get());
    }
    EXPECT_EQ(1, RefCountedBase::constructor_count());
    EXPECT_EQ(1, RefCountedBase::destructor_count());
    EXPECT_EQ(1, RefCountedDerived::constructor_count());
    EXPECT_EQ(1, RefCountedDerived::destructor_count());
  }
  EXPECT_EQ(1, RefCountedBase::constructor_count());
  EXPECT_EQ(1, RefCountedBase::destructor_count());
  EXPECT_EQ(1, RefCountedDerived::constructor_count());
  EXPECT_EQ(1, RefCountedDerived::destructor_count());
}

TEST(RefCountedTest, TestMakeRefCounted) {
  RefPtr<Derived> derived = new Derived;
  EXPECT_TRUE(derived->HasOneRef());
  derived.reset();

  RefPtr<Derived> derived2 = MakeRefCounted<Derived>();
  EXPECT_TRUE(derived2->HasOneRef());
  derived2.reset();
}

TEST(RefCountedTest, TestReset) {
  RefCountedBase::reset_count();

  // Create RefCountedBase that is referenced by |obj1| and |obj2|.
  RefPtr<RefCountedBase> obj1 = MakeRefCounted<RefCountedBase>();
  RefPtr<RefCountedBase> obj2 = obj1;
  EXPECT_NE(obj1.get(), nullptr);
  EXPECT_NE(obj2.get(), nullptr);
  EXPECT_EQ(RefCountedBase::constructor_count(), 1);
  EXPECT_EQ(RefCountedBase::destructor_count(), 0);

  // Check that calling reset() on |obj1| resets it. |obj2| still has a
  // reference to the RefCountedBase so it shouldn't be reset.
  obj1.reset();
  EXPECT_EQ(obj1.get(), nullptr);
  EXPECT_EQ(RefCountedBase::constructor_count(), 1);
  EXPECT_EQ(RefCountedBase::destructor_count(), 0);

  // Check that calling reset() on |obj2| resets it and causes the deletion of
  // the RefCountedBase.
  obj2.reset();
  EXPECT_EQ(obj2.get(), nullptr);
  EXPECT_EQ(RefCountedBase::constructor_count(), 1);
  EXPECT_EQ(RefCountedBase::destructor_count(), 1);
}

TEST(RefCountedTest, TestResetAlreadyNull) {
  // Check that calling reset() on a null RefPtr does nothing.
  RefPtr<RefCountedBase> obj;
  obj.reset();
  // |obj| should still be null after calling reset().
  EXPECT_EQ(obj.get(), nullptr);
}

TEST(RefCountedTest, TestResetByNullptrAssignment) {
  // Check that assigning nullptr resets the object.
  auto obj = MakeRefCounted<RefCountedBase>();
  EXPECT_NE(obj.get(), nullptr);

  obj = nullptr;
  EXPECT_EQ(obj.get(), nullptr);
}

TEST(RefCountedTest, CheckAutoRefptrNullBeforeObjectDestruction) {
  RefPtr<CheckRefCountedNULL> obj = MakeRefCounted<CheckRefCountedNULL>();
  obj->set_auto_refptr(&obj);

  // Check that when reset() is called the RefPtr internal pointer is
  // set to null before the reference counted object is destroyed. This check is
  // done by the CheckRefCountedNULL destructor.
  obj.reset();
  EXPECT_EQ(obj.get(), nullptr);
}

}  // namespace iomgr

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}