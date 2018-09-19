#include <OMR/IntrusiveList.hpp>
#include <gtest/gtest.h>

namespace OMR
{

struct LinkedInt;

using LinkedIntNode = IntrusiveListNode<LinkedInt>;
using LinkedIntList = IntrusiveList<LinkedInt>;
using LinkedIntListIterator = IntrusiveListIterator<LinkedInt>;

/// Forms a linked list of integers.
struct LinkedInt
{
	int value;
	LinkedIntNode node;
};

/// Specialization for pulling node out of LinkedInt.
template<>
struct GetIntrusiveListNode<LinkedInt>
{
	constexpr IntrusiveListNode<LinkedInt>& operator()(LinkedInt& i) const noexcept
	{
		return i.node;
	}
};

namespace Test
{

TEST(TestIntrusiveList, empty)
{
	LinkedIntList list;
	EXPECT_TRUE(list.empty());
}

TEST(TestIntrusiveList, addOne)
{
	LinkedIntList list;
	LinkedInt a = {1};
	list.add(&a);
	EXPECT_FALSE(list.empty());
}

TEST(TestIntrusiveList, iterateOne)
{
	LinkedIntList list;

	LinkedInt a = {1};
	list.add(&a);

	LinkedIntListIterator iter = list.begin();
	EXPECT_NE(iter, list.end());
	EXPECT_EQ(iter->value, 1);

	++iter;
	EXPECT_EQ(iter, list.end());
}

TEST(TestIntrusiveList, iterateTwo)
{
	LinkedIntList list;

	LinkedInt a = {1};
	list.add(&a);

	LinkedInt b = {2};
	list.add(&b);

	LinkedIntListIterator iter = list.begin();
	EXPECT_EQ(iter->value, 1);

	++iter;
	EXPECT_EQ(iter->value, 2);

	++iter;
	EXPECT_EQ(iter, list.end());
}

TEST(TestIntrusiveList, AddThenRemoveOne)
{
	LinkedIntList list;
	EXPECT_TRUE(list.empty());

	LinkedInt a = {1};
	list.add(&a);
	EXPECT_FALSE(list.empty());

	list.remove(&a);
	EXPECT_TRUE(list.empty());

	LinkedIntListIterator iter = list.begin();
	EXPECT_EQ(iter, list.end());
}

TEST(TestIntrusiveList, AddTwoThenRemoveOne)
{
	LinkedIntList list;
	LinkedInt a = {1};
	list.add(&a);
	LinkedInt b = {2};
	list.add(&b);
	list.remove(&a);

	LinkedIntListIterator iter = list.begin();
	EXPECT_NE(iter, list.end());
	EXPECT_EQ(iter->value, 2);
	++iter;
	EXPECT_EQ(iter, list.end());
}

} // namespace Test
} // namespace OMR
