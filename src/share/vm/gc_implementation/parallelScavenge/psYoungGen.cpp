/*
 * Copyright 2001-2007 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 */

# include "incls/_precompiled.incl"
# include "incls/_psYoungGen.cpp.incl"

PSYoungGen::PSYoungGen(size_t        initial_size,
                       size_t        min_size,
                       size_t        max_size) :
  _init_gen_size(initial_size),
  _min_gen_size(min_size),
  _max_gen_size(max_size)
{}

void PSYoungGen::initialize_virtual_space(ReservedSpace rs, size_t alignment) {
  assert(_init_gen_size != 0, "Should have a finite size");
  _virtual_space = new PSVirtualSpace(rs, alignment);
  if (!_virtual_space->expand_by(_init_gen_size)) {
    vm_exit_during_initialization("Could not reserve enough space for "
                                  "object heap");
  }
}

void PSYoungGen::initialize(ReservedSpace rs, size_t alignment) {
  initialize_virtual_space(rs, alignment);
  initialize_work();
}

void PSYoungGen::initialize_work() {

  _reserved = MemRegion((HeapWord*)_virtual_space->low_boundary(),
                        (HeapWord*)_virtual_space->high_boundary());

  MemRegion cmr((HeapWord*)_virtual_space->low(),
                (HeapWord*)_virtual_space->high());
  Universe::heap()->barrier_set()->resize_covered_region(cmr);

  if (UseNUMA) {
    _eden_space = new MutableNUMASpace();
  } else {
    _eden_space = new MutableSpace();
  }
  _from_space = new MutableSpace();
  _to_space   = new MutableSpace();

  if (_eden_space == NULL || _from_space == NULL || _to_space == NULL) {
    vm_exit_during_initialization("Could not allocate a young gen space");
  }

  // Allocate the mark sweep views of spaces
  _eden_mark_sweep =
      new PSMarkSweepDecorator(_eden_space, NULL, MarkSweepDeadRatio);
  _from_mark_sweep =
      new PSMarkSweepDecorator(_from_space, NULL, MarkSweepDeadRatio);
  _to_mark_sweep =
      new PSMarkSweepDecorator(_to_space, NULL, MarkSweepDeadRatio);

  if (_eden_mark_sweep == NULL ||
      _from_mark_sweep == NULL ||
      _to_mark_sweep == NULL) {
    vm_exit_during_initialization("Could not complete allocation"
                                  " of the young generation");
  }

  // Generation Counters - generation 0, 3 subspaces
  _gen_counters = new PSGenerationCounters("new", 0, 3, _virtual_space);

  // Compute maximum space sizes for performance counters
  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  size_t alignment = heap->intra_heap_alignment();
  size_t size = _virtual_space->reserved_size();

  size_t max_survivor_size;
  size_t max_eden_size;

  if (UseAdaptiveSizePolicy) {
    max_survivor_size = size / MinSurvivorRatio;

    // round the survivor space size down to the nearest alignment
    // and make sure its size is greater than 0.
    max_survivor_size = align_size_down(max_survivor_size, alignment);
    max_survivor_size = MAX2(max_survivor_size, alignment);

    // set the maximum size of eden to be the size of the young gen
    // less two times the minimum survivor size. The minimum survivor
    // size for UseAdaptiveSizePolicy is one alignment.
    max_eden_size = size - 2 * alignment;
  } else {
    max_survivor_size = size / InitialSurvivorRatio;

    // round the survivor space size down to the nearest alignment
    // and make sure its size is greater than 0.
    max_survivor_size = align_size_down(max_survivor_size, alignment);
    max_survivor_size = MAX2(max_survivor_size, alignment);

    // set the maximum size of eden to be the size of the young gen
    // less two times the survivor size when the generation is 100%
    // committed. The minimum survivor size for -UseAdaptiveSizePolicy
    // is dependent on the committed portion (current capacity) of the
    // generation - the less space committed, the smaller the survivor
    // space, possibly as small as an alignment. However, we are interested
    // in the case where the young generation is 100% committed, as this
    // is the point where eden reachs its maximum size. At this point,
    // the size of a survivor space is max_survivor_size.
    max_eden_size = size - 2 * max_survivor_size;
  }

  _eden_counters = new SpaceCounters("eden", 0, max_eden_size, _eden_space,
                                     _gen_counters);
  _from_counters = new SpaceCounters("s0", 1, max_survivor_size, _from_space,
                                     _gen_counters);
  _to_counters = new SpaceCounters("s1", 2, max_survivor_size, _to_space,
                                   _gen_counters);

  compute_initial_space_boundaries();
}

void PSYoungGen::compute_initial_space_boundaries() {
  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  assert(heap->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");

  // Compute sizes
  size_t alignment = heap->intra_heap_alignment();
  size_t size = _virtual_space->committed_size();

  size_t survivor_size = size / InitialSurvivorRatio;
  survivor_size = align_size_down(survivor_size, alignment);
  // ... but never less than an alignment
  survivor_size = MAX2(survivor_size, alignment);

  // Young generation is eden + 2 survivor spaces
  size_t eden_size = size - (2 * survivor_size);

  // Now go ahead and set 'em.
  set_space_boundaries(eden_size, survivor_size);
  space_invariants();

  if (UsePerfData) {
    _eden_counters->update_capacity();
    _from_counters->update_capacity();
    _to_counters->update_capacity();
  }
}

void PSYoungGen::set_space_boundaries(size_t eden_size, size_t survivor_size) {
  assert(eden_size < _virtual_space->committed_size(), "just checking");
  assert(eden_size > 0  && survivor_size > 0, "just checking");

  // Initial layout is Eden, to, from. After swapping survivor spaces,
  // that leaves us with Eden, from, to, which is step one in our two
  // step resize-with-live-data procedure.
  char *eden_start = _virtual_space->low();
  char *to_start   = eden_start + eden_size;
  char *from_start = to_start   + survivor_size;
  char *from_end   = from_start + survivor_size;

  assert(from_end == _virtual_space->high(), "just checking");
  assert(is_object_aligned((intptr_t)eden_start), "checking alignment");
  assert(is_object_aligned((intptr_t)to_start),   "checking alignment");
  assert(is_object_aligned((intptr_t)from_start), "checking alignment");

  MemRegion eden_mr((HeapWord*)eden_start, (HeapWord*)to_start);
  MemRegion to_mr  ((HeapWord*)to_start, (HeapWord*)from_start);
  MemRegion from_mr((HeapWord*)from_start, (HeapWord*)from_end);

  eden_space()->initialize(eden_mr, true);
    to_space()->initialize(to_mr  , true);
  from_space()->initialize(from_mr, true);
}

#ifndef PRODUCT
void PSYoungGen::space_invariants() {
  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  const size_t alignment = heap->intra_heap_alignment();

  // Currently, our eden size cannot shrink to zero
  guarantee(eden_space()->capacity_in_bytes() >= alignment, "eden too small");
  guarantee(from_space()->capacity_in_bytes() >= alignment, "from too small");
  guarantee(to_space()->capacity_in_bytes() >= alignment, "to too small");

  // Relationship of spaces to each other
  char* eden_start = (char*)eden_space()->bottom();
  char* eden_end   = (char*)eden_space()->end();
  char* from_start = (char*)from_space()->bottom();
  char* from_end   = (char*)from_space()->end();
  char* to_start   = (char*)to_space()->bottom();
  char* to_end     = (char*)to_space()->end();

  guarantee(eden_start >= _virtual_space->low(), "eden bottom");
  guarantee(eden_start < eden_end, "eden space consistency");
  guarantee(from_start < from_end, "from space consistency");
  guarantee(to_start < to_end, "to space consistency");

  // Check whether from space is below to space
  if (from_start < to_start) {
    // Eden, from, to
    guarantee(eden_end <= from_start, "eden/from boundary");
    guarantee(from_end <= to_start,   "from/to boundary");
    guarantee(to_end <= _virtual_space->high(), "to end");
  } else {
    // Eden, to, from
    guarantee(eden_end <= to_start, "eden/to boundary");
    guarantee(to_end <= from_start, "to/from boundary");
    guarantee(from_end <= _virtual_space->high(), "from end");
  }

  // More checks that the virtual space is consistent with the spaces
  assert(_virtual_space->committed_size() >=
    (eden_space()->capacity_in_bytes() +
     to_space()->capacity_in_bytes() +
     from_space()->capacity_in_bytes()), "Committed size is inconsistent");
  assert(_virtual_space->committed_size() <= _virtual_space->reserved_size(),
    "Space invariant");
  char* eden_top = (char*)eden_space()->top();
  char* from_top = (char*)from_space()->top();
  char* to_top = (char*)to_space()->top();
  assert(eden_top <= _virtual_space->high(), "eden top");
  assert(from_top <= _virtual_space->high(), "from top");
  assert(to_top <= _virtual_space->high(), "to top");

  _virtual_space->verify();
}
#endif

void PSYoungGen::resize(size_t eden_size, size_t survivor_size) {
  // Resize the generation if needed. If the generation resize
  // reports false, do not attempt to resize the spaces.
  if (resize_generation(eden_size, survivor_size)) {
    // Then we lay out the spaces inside the generation
    resize_spaces(eden_size, survivor_size);

    space_invariants();

    if (PrintAdaptiveSizePolicy && Verbose) {
      gclog_or_tty->print_cr("Young generation size: "
        "desired eden: " SIZE_FORMAT " survivor: " SIZE_FORMAT
        " used: " SIZE_FORMAT " capacity: " SIZE_FORMAT
        " gen limits: " SIZE_FORMAT " / " SIZE_FORMAT,
        eden_size, survivor_size, used_in_bytes(), capacity_in_bytes(),
        _max_gen_size, min_gen_size());
    }
  }
}


bool PSYoungGen::resize_generation(size_t eden_size, size_t survivor_size) {
  const size_t alignment = _virtual_space->alignment();
  size_t orig_size = _virtual_space->committed_size();
  bool size_changed = false;

  // There used to be this guarantee there.
  // guarantee ((eden_size + 2*survivor_size)  <= _max_gen_size, "incorrect input arguments");
  // Code below forces this requirement.  In addition the desired eden
  // size and disired survivor sizes are desired goals and may
  // exceed the total generation size.

  assert(min_gen_size() <= orig_size && orig_size <= max_size(), "just checking");

  // Adjust new generation size
  const size_t eden_plus_survivors =
          align_size_up(eden_size + 2 * survivor_size, alignment);
  size_t desired_size = MAX2(MIN2(eden_plus_survivors, max_size()),
                             min_gen_size());
  assert(desired_size <= max_size(), "just checking");

  if (desired_size > orig_size) {
    // Grow the generation
    size_t change = desired_size - orig_size;
    assert(change % alignment == 0, "just checking");
    if (!_virtual_space->expand_by(change)) {
      return false; // Error if we fail to resize!
    }

    size_changed = true;
  } else if (desired_size < orig_size) {
    size_t desired_change = orig_size - desired_size;
    assert(desired_change % alignment == 0, "just checking");

    desired_change = limit_gen_shrink(desired_change);

    if (desired_change > 0) {
      virtual_space()->shrink_by(desired_change);
      reset_survivors_after_shrink();

      size_changed = true;
    }
  } else {
    if (Verbose && PrintGC) {
      if (orig_size == gen_size_limit()) {
        gclog_or_tty->print_cr("PSYoung generation size at maximum: "
          SIZE_FORMAT "K", orig_size/K);
      } else if (orig_size == min_gen_size()) {
        gclog_or_tty->print_cr("PSYoung generation size at minium: "
          SIZE_FORMAT "K", orig_size/K);
      }
    }
  }

  if (size_changed) {
    post_resize();

    if (Verbose && PrintGC) {
      size_t current_size  = _virtual_space->committed_size();
      gclog_or_tty->print_cr("PSYoung generation size changed: "
                             SIZE_FORMAT "K->" SIZE_FORMAT "K",
                             orig_size/K, current_size/K);
    }
  }

  guarantee(eden_plus_survivors <= _virtual_space->committed_size() ||
            _virtual_space->committed_size() == max_size(), "Sanity");

  return true;
}


void PSYoungGen::resize_spaces(size_t requested_eden_size,
                               size_t requested_survivor_size) {
  assert(UseAdaptiveSizePolicy, "sanity check");
  assert(requested_eden_size > 0  && requested_survivor_size > 0,
         "just checking");

  // We require eden and to space to be empty
  if ((!eden_space()->is_empty()) || (!to_space()->is_empty())) {
    return;
  }

  if (PrintAdaptiveSizePolicy && Verbose) {
    gclog_or_tty->print_cr("PSYoungGen::resize_spaces(requested_eden_size: "
                  SIZE_FORMAT
                  ", requested_survivor_size: " SIZE_FORMAT ")",
                  requested_eden_size, requested_survivor_size);
    gclog_or_tty->print_cr("    eden: [" PTR_FORMAT ".." PTR_FORMAT ") "
                  SIZE_FORMAT,
                  eden_space()->bottom(),
                  eden_space()->end(),
                  pointer_delta(eden_space()->end(),
                                eden_space()->bottom(),
                                sizeof(char)));
    gclog_or_tty->print_cr("    from: [" PTR_FORMAT ".." PTR_FORMAT ") "
                  SIZE_FORMAT,
                  from_space()->bottom(),
                  from_space()->end(),
                  pointer_delta(from_space()->end(),
                                from_space()->bottom(),
                                sizeof(char)));
    gclog_or_tty->print_cr("      to: [" PTR_FORMAT ".." PTR_FORMAT ") "
                  SIZE_FORMAT,
                  to_space()->bottom(),
                  to_space()->end(),
                  pointer_delta(  to_space()->end(),
                                  to_space()->bottom(),
                                  sizeof(char)));
  }

  // There's nothing to do if the new sizes are the same as the current
  if (requested_survivor_size == to_space()->capacity_in_bytes() &&
      requested_survivor_size == from_space()->capacity_in_bytes() &&
      requested_eden_size == eden_space()->capacity_in_bytes()) {
    if (PrintAdaptiveSizePolicy && Verbose) {
      gclog_or_tty->print_cr("    capacities are the right sizes, returning");
    }
    return;
  }

  char* eden_start = (char*)eden_space()->bottom();
  char* eden_end   = (char*)eden_space()->end();
  char* from_start = (char*)from_space()->bottom();
  char* from_end   = (char*)from_space()->end();
  char* to_start   = (char*)to_space()->bottom();
  char* to_end     = (char*)to_space()->end();

  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  const size_t alignment = heap->intra_heap_alignment();
  const bool maintain_minimum =
    (requested_eden_size + 2 * requested_survivor_size) <= min_gen_size();

  // Check whether from space is below to space
  if (from_start < to_start) {
    // Eden, from, to
    if (PrintAdaptiveSizePolicy && Verbose) {
      gclog_or_tty->print_cr("  Eden, from, to:");
    }

    // Set eden
    // "requested_eden_size" is a goal for the size of eden
    // and may not be attainable.  "eden_size" below is
    // calculated based on the location of from-space and
    // the goal for the size of eden.  from-space is
    // fixed in place because it contains live data.
    // The calculation is done this way to avoid 32bit
    // overflow (i.e., eden_start + requested_eden_size
    // may too large for representation in 32bits).
    size_t eden_size;
    if (maintain_minimum) {
      // Only make eden larger than the requested size if
      // the minimum size of the generation has to be maintained.
      // This could be done in general but policy at a higher
      // level is determining a requested size for eden and that
      // should be honored unless there is a fundamental reason.
      eden_size = pointer_delta(from_start,
                                eden_start,
                                sizeof(char));
    } else {
      eden_size = MIN2(requested_eden_size,
                       pointer_delta(from_start, eden_start, sizeof(char)));
    }

    eden_end = eden_start + eden_size;
    assert(eden_end >= eden_start, "addition overflowed")

    // To may resize into from space as long as it is clear of live data.
    // From space must remain page aligned, though, so we need to do some
    // extra calculations.

    // First calculate an optimal to-space
    to_end   = (char*)_virtual_space->high();
    to_start = (char*)pointer_delta(to_end, (char*)requested_survivor_size,
                                    sizeof(char));

    // Does the optimal to-space overlap from-space?
    if (to_start < (char*)from_space()->end()) {
      assert(heap->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");

      // Calculate the minimum offset possible for from_end
      size_t from_size = pointer_delta(from_space()->top(), from_start, sizeof(char));

      // Should we be in this method if from_space is empty? Why not the set_space method? FIX ME!
      if (from_size == 0) {
        from_size = alignment;
      } else {
        from_size = align_size_up(from_size, alignment);
      }

      from_end = from_start + from_size;
      assert(from_end > from_start, "addition overflow or from_size problem");

      guarantee(from_end <= (char*)from_space()->end(), "from_end moved to the right");

      // Now update to_start with the new from_end
      to_start = MAX2(from_end, to_start);
    }

    guarantee(to_start != to_end, "to space is zero sized");

    if (PrintAdaptiveSizePolicy && Verbose) {
      gclog_or_tty->print_cr("    [eden_start .. eden_end): "
                    "[" PTR_FORMAT " .. " PTR_FORMAT ") " SIZE_FORMAT,
                    eden_start,
                    eden_end,
                    pointer_delta(eden_end, eden_start, sizeof(char)));
      gclog_or_tty->print_cr("    [from_start .. from_end): "
                    "[" PTR_FORMAT " .. " PTR_FORMAT ") " SIZE_FORMAT,
                    from_start,
                    from_end,
                    pointer_delta(from_end, from_start, sizeof(char)));
      gclog_or_tty->print_cr("    [  to_start ..   to_end): "
                    "[" PTR_FORMAT " .. " PTR_FORMAT ") " SIZE_FORMAT,
                    to_start,
                    to_end,
                    pointer_delta(  to_end,   to_start, sizeof(char)));
    }
  } else {
    // Eden, to, from
    if (PrintAdaptiveSizePolicy && Verbose) {
      gclog_or_tty->print_cr("  Eden, to, from:");
    }

    // To space gets priority over eden resizing. Note that we position
    // to space as if we were able to resize from space, even though from
    // space is not modified.
    // Giving eden priority was tried and gave poorer performance.
    to_end   = (char*)pointer_delta(_virtual_space->high(),
                                    (char*)requested_survivor_size,
                                    sizeof(char));
    to_end   = MIN2(to_end, from_start);
    to_start = (char*)pointer_delta(to_end, (char*)requested_survivor_size,
                                    sizeof(char));
    // if the space sizes are to be increased by several times then
    // 'to_start' will point beyond the young generation. In this case
    // 'to_start' should be adjusted.
    to_start = MAX2(to_start, eden_start + alignment);

    // Compute how big eden can be, then adjust end.
    // See  comments above on calculating eden_end.
    size_t eden_size;
    if (maintain_minimum) {
      eden_size = pointer_delta(to_start, eden_start, sizeof(char));
    } else {
      eden_size = MIN2(requested_eden_size,
                       pointer_delta(to_start, eden_start, sizeof(char)));
    }
    eden_end = eden_start + eden_size;
    assert(eden_end >= eden_start, "addition overflowed")

    // Could choose to not let eden shrink
    // to_start = MAX2(to_start, eden_end);

    // Don't let eden shrink down to 0 or less.
    eden_end = MAX2(eden_end, eden_start + alignment);
    to_start = MAX2(to_start, eden_end);

    if (PrintAdaptiveSizePolicy && Verbose) {
      gclog_or_tty->print_cr("    [eden_start .. eden_end): "
                    "[" PTR_FORMAT " .. " PTR_FORMAT ") " SIZE_FORMAT,
                    eden_start,
                    eden_end,
                    pointer_delta(eden_end, eden_start, sizeof(char)));
      gclog_or_tty->print_cr("    [  to_start ..   to_end): "
                    "[" PTR_FORMAT " .. " PTR_FORMAT ") " SIZE_FORMAT,
                    to_start,
                    to_end,
                    pointer_delta(  to_end,   to_start, sizeof(char)));
      gclog_or_tty->print_cr("    [from_start .. from_end): "
                    "[" PTR_FORMAT " .. " PTR_FORMAT ") " SIZE_FORMAT,
                    from_start,
                    from_end,
                    pointer_delta(from_end, from_start, sizeof(char)));
    }
  }


  guarantee((HeapWord*)from_start <= from_space()->bottom(),
            "from start moved to the right");
  guarantee((HeapWord*)from_end >= from_space()->top(),
            "from end moved into live data");
  assert(is_object_aligned((intptr_t)eden_start), "checking alignment");
  assert(is_object_aligned((intptr_t)from_start), "checking alignment");
  assert(is_object_aligned((intptr_t)to_start), "checking alignment");

  MemRegion edenMR((HeapWord*)eden_start, (HeapWord*)eden_end);
  MemRegion toMR  ((HeapWord*)to_start,   (HeapWord*)to_end);
  MemRegion fromMR((HeapWord*)from_start, (HeapWord*)from_end);

  // Let's make sure the call to initialize doesn't reset "top"!
  HeapWord* old_from_top = from_space()->top();

  // For PrintAdaptiveSizePolicy block  below
  size_t old_from = from_space()->capacity_in_bytes();
  size_t old_to   = to_space()->capacity_in_bytes();

  eden_space()->initialize(edenMR, true);
    to_space()->initialize(toMR  , true);
  from_space()->initialize(fromMR, false);     // Note, not cleared!

  assert(from_space()->top() == old_from_top, "from top changed!");

  if (PrintAdaptiveSizePolicy) {
    ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
    assert(heap->kind() == CollectedHeap::ParallelScavengeHeap, "Sanity");

    gclog_or_tty->print("AdaptiveSizePolicy::survivor space sizes: "
                  "collection: %d "
                  "(" SIZE_FORMAT ", " SIZE_FORMAT ") -> "
                  "(" SIZE_FORMAT ", " SIZE_FORMAT ") ",
                  heap->total_collections(),
                  old_from, old_to,
                  from_space()->capacity_in_bytes(),
                  to_space()->capacity_in_bytes());
    gclog_or_tty->cr();
  }
}

void PSYoungGen::swap_spaces() {
  MutableSpace* s    = from_space();
  _from_space        = to_space();
  _to_space          = s;

  // Now update the decorators.
  PSMarkSweepDecorator* md = from_mark_sweep();
  _from_mark_sweep           = to_mark_sweep();
  _to_mark_sweep             = md;

  assert(from_mark_sweep()->space() == from_space(), "Sanity");
  assert(to_mark_sweep()->space() == to_space(), "Sanity");
}

size_t PSYoungGen::capacity_in_bytes() const {
  return eden_space()->capacity_in_bytes()
       + from_space()->capacity_in_bytes();  // to_space() is only used during scavenge
}


size_t PSYoungGen::used_in_bytes() const {
  return eden_space()->used_in_bytes()
       + from_space()->used_in_bytes();      // to_space() is only used during scavenge
}


size_t PSYoungGen::free_in_bytes() const {
  return eden_space()->free_in_bytes()
       + from_space()->free_in_bytes();      // to_space() is only used during scavenge
}

size_t PSYoungGen::capacity_in_words() const {
  return eden_space()->capacity_in_words()
       + from_space()->capacity_in_words();  // to_space() is only used during scavenge
}


size_t PSYoungGen::used_in_words() const {
  return eden_space()->used_in_words()
       + from_space()->used_in_words();      // to_space() is only used during scavenge
}


size_t PSYoungGen::free_in_words() const {
  return eden_space()->free_in_words()
       + from_space()->free_in_words();      // to_space() is only used during scavenge
}

void PSYoungGen::object_iterate(ObjectClosure* blk) {
  eden_space()->object_iterate(blk);
  from_space()->object_iterate(blk);
  to_space()->object_iterate(blk);
}

void PSYoungGen::precompact() {
  eden_mark_sweep()->precompact();
  from_mark_sweep()->precompact();
  to_mark_sweep()->precompact();
}

void PSYoungGen::adjust_pointers() {
  eden_mark_sweep()->adjust_pointers();
  from_mark_sweep()->adjust_pointers();
  to_mark_sweep()->adjust_pointers();
}

void PSYoungGen::compact() {
  eden_mark_sweep()->compact(ZapUnusedHeapArea);
  from_mark_sweep()->compact(ZapUnusedHeapArea);
  // Mark sweep stores preserved markOops in to space, don't disturb!
  to_mark_sweep()->compact(false);
}

void PSYoungGen::move_and_update(ParCompactionManager* cm) {
  PSParallelCompact::move_and_update(cm, PSParallelCompact::eden_space_id);
  PSParallelCompact::move_and_update(cm, PSParallelCompact::from_space_id);
  PSParallelCompact::move_and_update(cm, PSParallelCompact::to_space_id);
}

void PSYoungGen::print() const { print_on(tty); }
void PSYoungGen::print_on(outputStream* st) const {
  st->print(" %-15s", "PSYoungGen");
  if (PrintGCDetails && Verbose) {
    st->print(" total " SIZE_FORMAT ", used " SIZE_FORMAT,
               capacity_in_bytes(), used_in_bytes());
  } else {
    st->print(" total " SIZE_FORMAT "K, used " SIZE_FORMAT "K",
               capacity_in_bytes()/K, used_in_bytes()/K);
  }
  _virtual_space->print_space_boundaries_on(st);
  st->print("  eden"); eden_space()->print_on(st);
  st->print("  from"); from_space()->print_on(st);
  st->print("  to  "); to_space()->print_on(st);
}

void PSYoungGen::print_used_change(size_t prev_used) const {
  gclog_or_tty->print(" [%s:", name());
  gclog_or_tty->print(" "  SIZE_FORMAT "K"
                      "->" SIZE_FORMAT "K"
                      "("  SIZE_FORMAT "K)",
                      prev_used / K, used_in_bytes() / K,
                      capacity_in_bytes() / K);
  gclog_or_tty->print("]");
}

size_t PSYoungGen::available_for_expansion() {
  ShouldNotReachHere();
  return 0;
}

size_t PSYoungGen::available_for_contraction() {
  ShouldNotReachHere();
  return 0;
}

size_t PSYoungGen::available_to_min_gen() {
  assert(virtual_space()->committed_size() >= min_gen_size(), "Invariant");
  return virtual_space()->committed_size() - min_gen_size();
}

// This method assumes that from-space has live data and that
// any shrinkage of the young gen is limited by location of
// from-space.
size_t PSYoungGen::available_to_live() {
  size_t delta_in_survivor = 0;
  ParallelScavengeHeap* heap = (ParallelScavengeHeap*)Universe::heap();
  const size_t space_alignment = heap->intra_heap_alignment();
  const size_t gen_alignment = heap->young_gen_alignment();

  MutableSpace* space_shrinking = NULL;
  if (from_space()->end() > to_space()->end()) {
    space_shrinking = from_space();
  } else {
    space_shrinking = to_space();
  }

  // Include any space that is committed but not included in
  // the survivor spaces.
  assert(((HeapWord*)virtual_space()->high()) >= space_shrinking->end(),
    "Survivor space beyond high end");
  size_t unused_committed = pointer_delta(virtual_space()->high(),
    space_shrinking->end(), sizeof(char));

  if (space_shrinking->is_empty()) {
    // Don't let the space shrink to 0
    assert(space_shrinking->capacity_in_bytes() >= space_alignment,
      "Space is too small");
    delta_in_survivor = space_shrinking->capacity_in_bytes() - space_alignment;
  } else {
    delta_in_survivor = pointer_delta(space_shrinking->end(),
                                      space_shrinking->top(),
                                      sizeof(char));
  }

  size_t delta_in_bytes = unused_committed + delta_in_survivor;
  delta_in_bytes = align_size_down(delta_in_bytes, gen_alignment);
  return delta_in_bytes;
}

// Return the number of bytes available for resizing down the young
// generation.  This is the minimum of
//      input "bytes"
//      bytes to the minimum young gen size
//      bytes to the size currently being used + some small extra
size_t PSYoungGen::limit_gen_shrink(size_t bytes) {
  // Allow shrinkage into the current eden but keep eden large enough
  // to maintain the minimum young gen size
  bytes = MIN3(bytes, available_to_min_gen(), available_to_live());
  return align_size_down(bytes, virtual_space()->alignment());
}

void PSYoungGen::reset_after_change() {
  ShouldNotReachHere();
}

void PSYoungGen::reset_survivors_after_shrink() {
  _reserved = MemRegion((HeapWord*)virtual_space()->low_boundary(),
                        (HeapWord*)virtual_space()->high_boundary());
  PSScavenge::reference_processor()->set_span(_reserved);

  MutableSpace* space_shrinking = NULL;
  if (from_space()->end() > to_space()->end()) {
    space_shrinking = from_space();
  } else {
    space_shrinking = to_space();
  }

  HeapWord* new_end = (HeapWord*)virtual_space()->high();
  assert(new_end >= space_shrinking->bottom(), "Shrink was too large");
  // Was there a shrink of the survivor space?
  if (new_end < space_shrinking->end()) {
    MemRegion mr(space_shrinking->bottom(), new_end);
    space_shrinking->initialize(mr, false /* clear */);
  }
}

// This method currently does not expect to expand into eden (i.e.,
// the virtual space boundaries is expected to be consistent
// with the eden boundaries..
void PSYoungGen::post_resize() {
  assert_locked_or_safepoint(Heap_lock);
  assert((eden_space()->bottom() < to_space()->bottom()) &&
         (eden_space()->bottom() < from_space()->bottom()),
         "Eden is assumed to be below the survivor spaces");

  MemRegion cmr((HeapWord*)virtual_space()->low(),
                (HeapWord*)virtual_space()->high());
  Universe::heap()->barrier_set()->resize_covered_region(cmr);
  space_invariants();
}



void PSYoungGen::update_counters() {
  if (UsePerfData) {
    _eden_counters->update_all();
    _from_counters->update_all();
    _to_counters->update_all();
    _gen_counters->update_all();
  }
}

void PSYoungGen::verify(bool allow_dirty) {
  eden_space()->verify(allow_dirty);
  from_space()->verify(allow_dirty);
  to_space()->verify(allow_dirty);
}
