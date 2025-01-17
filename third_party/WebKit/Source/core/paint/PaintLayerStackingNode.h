/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifndef PaintLayerStackingNode_h
#define PaintLayerStackingNode_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class PaintLayer;
class PaintLayerCompositor;
class ComputedStyle;
class LayoutBoxModelObject;

// PaintLayerStackingNode represents anything that is a stacking
// context or treated as a stacking context.
//
// Stacking contexts are the basis for the CSS painting algorithm. The paint
// order is determined by walking stacking contexts (or elements treated like a
// stacking context like positioned objects or floats) in an order defined by
// ‘z-index’. This walk is interleaved with content that is not a stacking.
// context. See CSS 2.1 appendix E for the actual algorithm
// http://www.w3.org/TR/CSS21/zindex.html
// See also PaintLayerPainter (in particular paintLayerContents) for
// our implementation of the walk.
//
// Stacking contexts form a subtree over the layout tree. Ideally we would want
// objects of this class to be a node in this tree but there are potential
// issues with stale pointers so we rely on PaintLayer's tree
// structure.
//
// This class's purpose is to represent a node in the stacking context tree
// (aka paint tree). It currently caches the z-order lists for painting and
// hit-testing.
//
// To implement any z-order list iterations, use
// PaintLayerStackingNodeIterator and
// PaintLayerStackingNodeReverseIterator.
class CORE_EXPORT PaintLayerStackingNode {
    WTF_MAKE_FAST_ALLOCATED(PaintLayerStackingNode);
    WTF_MAKE_NONCOPYABLE(PaintLayerStackingNode);
public:
    explicit PaintLayerStackingNode(PaintLayer*);
    ~PaintLayerStackingNode();

    int zIndex() const { return layoutObject()->style()->zIndex(); }

    bool isStackingContext() const { return layoutObject()->style()->isStackingContext(); }

    // Update our normal and z-index lists.
    void updateLayerListsIfNeeded();

    bool zOrderListsDirty() const { return m_zOrderListsDirty; }
    void dirtyZOrderLists();
    void updateZOrderLists();
    void clearZOrderLists();
    void dirtyStackingContextZOrderLists();

    bool hasPositiveZOrderList() const { return posZOrderList() && posZOrderList()->size(); }
    bool hasNegativeZOrderList() const { return negZOrderList() && negZOrderList()->size(); }

    bool isTreatedAsOrStackingContext() const { return m_isTreatedAsOrStackingContext; }
    void updateIsTreatedAsStackingContext();

    void updateStackingNodesAfterStyleChange(const ComputedStyle* oldStyle);

    PaintLayerStackingNode* ancestorStackingContextNode() const;

    PaintLayer* layer() const { return m_layer; }

#if ENABLE(ASSERT)
    bool layerListMutationAllowed() const { return m_layerListMutationAllowed; }
    void setLayerListMutationAllowed(bool flag) { m_layerListMutationAllowed = flag; }
#endif

private:
    friend class PaintLayerStackingNodeIterator;
    friend class PaintLayerStackingNodeReverseIterator;
    friend class LayoutTreeAsText;

    Vector<PaintLayerStackingNode*>* posZOrderList() const
    {
        ASSERT(!m_zOrderListsDirty);
        ASSERT(isStackingContext() || !m_posZOrderList);
        return m_posZOrderList.get();
    }

    Vector<PaintLayerStackingNode*>* negZOrderList() const
    {
        ASSERT(!m_zOrderListsDirty);
        ASSERT(isStackingContext() || !m_negZOrderList);
        return m_negZOrderList.get();
    }

    void rebuildZOrderLists();
    void collectLayers(OwnPtr<Vector<PaintLayerStackingNode*>>& posZOrderList, OwnPtr<Vector<PaintLayerStackingNode*>>& negZOrderList);

#if ENABLE(ASSERT)
    bool isInStackingParentZOrderLists() const;
    void updateStackingParentForZOrderLists(PaintLayerStackingNode* stackingParent);
    void setStackingParent(PaintLayerStackingNode* stackingParent) { m_stackingParent = stackingParent; }
#endif

    bool shouldBeTreatedAsOrStackingContext() const { return layoutObject()->style()->isTreatedAsOrStackingContext(); }

    bool isDirtyStackingContext() const { return m_zOrderListsDirty && isStackingContext(); }

    PaintLayerCompositor* compositor() const;
    // We can't return a LayoutBox as LayoutInline can be a stacking context.
    LayoutBoxModelObject* layoutObject() const;

    PaintLayer* m_layer;

    // m_posZOrderList holds a sorted list of all the descendant nodes within
    // that have z-indices of 0 or greater (auto will count as 0).
    // m_negZOrderList holds descendants within our stacking context with
    // negative z-indices.
    OwnPtr<Vector<PaintLayerStackingNode*>> m_posZOrderList;
    OwnPtr<Vector<PaintLayerStackingNode*>> m_negZOrderList;

    // This boolean caches whether the z-order lists above are dirty.
    // It is only ever set for stacking contexts, as no other element can
    // have z-order lists.
    unsigned m_zOrderListsDirty : 1;

    // This attribute caches whether the element was a stacking context or
    // was treated like a stacking context, so that we can do comparison against
    // it during style change (styleDidChange in particular), as we have lost
    // the previous style information.
    unsigned m_isTreatedAsOrStackingContext: 1;

#if ENABLE(ASSERT)
    unsigned m_layerListMutationAllowed : 1;
    PaintLayerStackingNode* m_stackingParent;
#endif
};

inline void PaintLayerStackingNode::clearZOrderLists()
{
    ASSERT(!isStackingContext());

#if ENABLE(ASSERT)
    updateStackingParentForZOrderLists(0);
#endif

    m_posZOrderList.clear();
    m_negZOrderList.clear();
}

inline void PaintLayerStackingNode::updateZOrderLists()
{
    if (!m_zOrderListsDirty)
        return;

    if (!isStackingContext()) {
        clearZOrderLists();
        m_zOrderListsDirty = false;
        return;
    }

    rebuildZOrderLists();
}

#if ENABLE(ASSERT)
class LayerListMutationDetector {
public:
    explicit LayerListMutationDetector(PaintLayerStackingNode* stackingNode)
        : m_stackingNode(stackingNode)
        , m_previousMutationAllowedState(stackingNode->layerListMutationAllowed())
    {
        m_stackingNode->setLayerListMutationAllowed(false);
    }

    ~LayerListMutationDetector()
    {
        m_stackingNode->setLayerListMutationAllowed(m_previousMutationAllowedState);
    }

private:
    PaintLayerStackingNode* m_stackingNode;
    bool m_previousMutationAllowedState;
};
#endif

} // namespace blink

#endif // PaintLayerStackingNode_h
