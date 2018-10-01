
#if !defined(OMR_INTRUSIVELIST_HPP_)
#define OMR_INTRUSIVELIST_HPP_

#include <type_traits>
#include <utility>
#include <cassert>

namespace OMR
{

template<typename T, typename Traits>
class IntrusiveList;

template<typename T, typename Traits>
class IntrusiveListIterator;

/// Stores the next and previous pointers for elements in an IntrusiveList.
/// Each element in an intrusive list must contain an IntrusiveListNode.
/// Elements must make their node accessible through GetIntrusiveListNode::operator()(T&).
template<typename T>
class IntrusiveListNode
{
public:
	constexpr IntrusiveListNode() : prev(nullptr), next(nullptr) {}

	/// assign previous, next to node.
	void assign(T* p, T* n) noexcept
	{
		prev = p;
		next = n;
	}

	// deactivate the node, and clear the next/prev pointers.
	void clear() noexcept
	{
		prev = nullptr;
		next = nullptr;
	}

	T* prev;
	T* next;
};

template<typename T>
struct IntrusiveListTraits
{
	using Node = IntrusiveListNode<T>;

	static constexpr Node& node(T& element) noexcept { return element.node(); }

	static constexpr const Node& node(const T& element) noexcept { return element.node(); }
};

/// Simple forward iterator for the elements in an intrusive list.
template<typename T, typename Traits>
class IntrusiveListIterator
{
public:
	IntrusiveListIterator(const IntrusiveListIterator<T, Traits>&) = default;

	constexpr T& operator*() const noexcept { return *_current; }

	constexpr T* operator->() const noexcept { return _current; }

	IntrusiveListIterator<T, Traits>& operator++() noexcept
	{
		_current = Traits::node(*_current).next;
		return *this;
	}

	constexpr bool operator==(const IntrusiveListIterator<T, Traits>& rhs) const noexcept
	{
		return _current == rhs._current;
	}

	constexpr bool operator!=(const IntrusiveListIterator<T, Traits>& rhs) const noexcept
	{
		return _current != rhs._current;
	}

	IntrusiveListIterator<T, Traits>&
	operator=(const IntrusiveListIterator<T, Traits>& rhs) noexcept
	{
		_current = rhs._current;
		return *this;
	}

private:
	friend class IntrusiveList<T, Traits>;

	constexpr IntrusiveListIterator(T* root) : _current(root) {}

	T* _current;
};

/// A linked list, where the next and previous pointers are stored in a node, which is embedded in the element type T.
///
/// To use an intrusive list, the element type T must store an IntrusiveListNode<T>.
/// The default policy will use T::node() to access the list node.
/// To override this behaviour, users can provide a custom list trait, or specialize the IntrusiveListTraits<T> for th
///
template<typename T, typename Traits = IntrusiveListTraits<T>>
class IntrusiveList
{
public:
	using Node = IntrusiveListNode<T>;

	using Iterator = IntrusiveListIterator<T, Traits>;

	constexpr IntrusiveList() noexcept : _root(nullptr) {}

	/// Add element to the head of the list. Constant time.
	void add(T* element) noexcept
	{
		assert(Traits::node(*element).next == nullptr);
		assert(Traits::node(*element).prev == nullptr);
	
		Traits::node(*element).assign(nullptr, _root);
		if (_root) {
			Traits::node(*_root).prev = element;
		}
		_root = element;
	}

	/// Remove element from the list. Removing an element invalidates any iterators. Constant time.
	void remove(T* element) noexcept
	{
		Node& node = Traits::node(*element);
		if (element == _root) {
			assert(node.prev == nullptr);
			if (node.next != nullptr) {
				Traits::node(*node.next).prev = nullptr;
				_root = node.next;
			}
			else {
				_root = nullptr;
			}
		} else {
			assert(node.prev != nullptr);
			Traits::node(*node.prev).next = node.next;
			if (node.next != nullptr) {
				Traits::node(*node.next).prev = node.prev;
			}
		}
		node.clear();
	}

	constexpr Iterator begin() const noexcept { return Iterator(_root); }

	constexpr Iterator end() const noexcept { return Iterator(nullptr); }

	constexpr bool empty() const noexcept { return _root == nullptr; }

private:
	T* _root;
};

} // namespace OMR

#endif // OMR_INTRUSIVELIST_HPP_
