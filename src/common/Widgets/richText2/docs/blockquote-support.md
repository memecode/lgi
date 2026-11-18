# Rte2 DivBlock Container Support

Target file:
`lgi/trunk/src/common/Widgets/richText2/docs/blockquote-support.md`


## Building

To build, use either:
- /home/matthew/code/lgi/trunk/test/HtmlEditor/linux/Makefile.linux
- /home/matthew/code/scribe/trunk_os/linux/Makefile.linux

## Goal

Support `<blockquote>` as a real node in the Rte2 block tree by using a generic container block, `DivBlock`.

`BlockQuoteBlock` is not the main abstraction. The better model is:

```text
DivBlock
  owns child Block objects
  owns CSS box properties: margin, border, padding
  remembers its HTML element name, for example div or blockquote
```

A blockquote can then be represented as a `DivBlock` whose stored element name is `blockquote`:

```text
DivBlock blockquote
  TextBlock: quoted text
  ListBlock
    TextBlock: nested item
```

For a normal styled div:

```text
DivBlock div
  TextBlock: content
```

If a separate `BlockQuoteBlock` class is useful for debug output, it should be only a thin subclass or factory wrapper around `DivBlock`, not the primary implementation.

## Why DivBlock

A generic `DivBlock` is better than a quote-only block because the same container behavior applies to HTML elements that wrap other block content:

```html
<div>
  <p>first</p>
  <ul><li>second</li></ul>
</div>
```

or:

```html
<blockquote>
  <p>first</p>
  <ul><li>second</li></ul>
</blockquote>
```

These need a tree node that can:

- contain child `Block` objects;
- apply CSS `margin`, `border`, and `padding` as a container box;
- preserve the original HTML tag on serialization;
- support layout, paint, hit-test, selection, copy, find, paste, undo, and split behavior.

A quote-specific `BlockQuoteBlock` can solve the immediate blockquote case, but it duplicates what a generic container should already provide.

## Recommended Class

```text
class DivBlock : public Block, public LCssBox<LRect>
{
    LNamedStyle *Style = nullptr;
    int TagId = 0;
    const char *ElementName = nullptr;

    LRect Pos;

public:
    const char *GetClass() override;
    const char *GetElementName();
    ...
};
```

`GetClass()` should remain an internal class name, not the HTML tag.

Recommended:

```text
DivBlock::GetClass() returns "DivBlock"
```

For debug output, `DumpNodes()` can show the stored element name.

The HTML tag should be stored separately:

```text
TagId
ElementName
```

`ElementName` can be resolved from `HtmlStatic` when possible, but storing it in the block makes serialization reliable.

Default:

```text
TagId = TAG_DIV
ElementName = "div"
```

For blockquote:

```text
TagId = TAG_BLOCKQUOTE
ElementName = "blockquote"
```

This is the key point: a `DivBlock` must know which HTML element it represents.

## Class Name vs HTML Element Name

Do not overload `GetClass()` as the HTML tag.

Possible confusion:

```text
GetClass() = "BlockQuoteBlock"
HTML output = <blockquote>
```

But for a generic `DivBlock`, the class name may be `DivBlock` while the HTML element is:

```text
div
blockquote
```

Recommended separation:

```text
GetClass()          internal tree class
GetElementName()    HTML tag to serialize
TagId               original parser tag id
```

If debug output wants a quote-specific label, `DumpNodes()` can show:

```text
DivBlock blockquote
```

or, optionally:

```text
BlockQuoteBlock
```

But serialization should use `GetElementName()`, not `GetClass()`.

## Optional Thin BlockQuoteBlock

If we want existing debug output to show `BlockQuoteBlock`, a thin subclass is acceptable:

```text
BlockQuoteBlock : public DivBlock
```

It should not duplicate layout, paint, selection, or editing logic.

It should only provide:

```text
GetClass() override, returning "BlockQuoteBlock"
Default blockquote CSS
Default ElementName = "blockquote"
```

However, the primary design should remain generic:

```text
BlockQuoteBlock = DivBlock with ElementName "blockquote"
```

## Parser Design

The parser should handle selected block-level container tags specially, especially:

```text
TAG_BLOCKQUOTE
```

For `TAG_BLOCKQUOTE`, create a `DivBlock` and record the tag.

Example parser behavior:

```text
Before:
ctx.TargetBlocks = &d->Blocks

Process:
QuoteBlock = new DivBlock(this)
QuoteBlock->TagId = TAG_BLOCKQUOTE
QuoteBlock->ElementName = "blockquote"
QuoteBlock->SetStyle(CachedStyle)
ctx.AddBlock(QuoteBlock)

ctx.TargetBlocks = &QuoteBlock->blocks
ctx.TargetParent = QuoteBlock

Process child content:
TextBlock "quoted" added inside QuoteBlock

Restore:
ctx.TargetBlocks = &d->Blocks
ctx.TargetParent = null
```

For a generic div:

```text
DivBlock = new DivBlock(this)
DivBlock->TagId = TAG_DIV
DivBlock->ElementName = "div"
DivBlock->SetStyle(CachedStyle)
```

Ensure at least one editable child `TextBlock` exists so the container can receive a cursor.

Example:

```html
<blockquote>quoted</blockquote>
```

Expected tree:

```text
DivBlock blockquote
  TextBlock: quoted
```

Nested example:

```html
<blockquote>
  <p>first</p>
  <ul><li>second</li></ul>
  <blockquote>third</blockquote>
</blockquote>
```

Expected tree:

```text
DivBlock blockquote
  TextBlock: first
  ListBlock
    TextBlock: second
  DivBlock blockquote
    TextBlock: third
```

## Parser Code Sketch

Inside `FromHtml`, after style parsing, add a container special case.

```cpp
if (c->TagId == TAG_BLOCKQUOTE || c->TagId == TAG_DIV)
{
    auto Container = new DivBlock(this);
    if (!Container)
        return Error(_FL, "Failed to create container block.");

    Container->TagId = c->TagId;
    Container->ElementName = c->TagId == TAG_BLOCKQUOTE ? "blockquote" : "div";
    Container->SetStyle(CachedStyle);

    if (ctx.Tb && ctx.LastChar != '\n')
    {
        const uint32_t NewLine[] = {'\n', 0};
        if (!ctx.Tb->AddText(NoTransaction, -1, NewLine, 1))
            return Error(_FL, "Failed to end the paragraph before a container block.");
    }

    if (!ctx.AddBlock(Container))
    {
        DeleteObj(Container);
        return Error(_FL, "Failed to add container block.");
    }

    auto PreviousTarget = ctx.TargetBlocks;
    auto PreviousParent = ctx.TargetParent;
    auto PreviousText = ctx.Tb;
    uint32_t PreviousLastChar = ctx.LastChar;
    bool PreviousStartOfLine = ctx.StartOfLine;

    ctx.TargetBlocks = &Container->blocks;
    ctx.TargetParent = Container;
    ctx.Tb = nullptr;

    LNamedStyle *ChildStyle = Container->MakeChildStyle(CachedStyle);

    if (!FromHtml(c, ctx, ChildStyle, Depth + 1))
    {
        ctx.TargetBlocks = PreviousTarget;
        ctx.TargetParent = PreviousParent;
        ctx.Tb = PreviousText;
        ctx.LastChar = PreviousLastChar;
        ctx.StartOfLine = PreviousStartOfLine;
        return false;
    }

    if (!Container->blocks.Length())
    {
        auto Empty = new TextBlock(this);
        if (!Container->Add(Empty))
            DeleteObj(Empty);
    }

    ctx.TargetBlocks = PreviousTarget;
    ctx.TargetParent = PreviousParent;
    ctx.Tb = nullptr;
    ctx.LastChar = '\n';
    ctx.StartOfLine = true;
    continue;
}
```

`MakeChildStyle()` should copy the parent style but clear `margin`, `border`, and `padding`, because the container owns the box properties.

Child blocks may inherit font and inline text properties, but not the container box properties.

## Default CSS

The `div` container should have no default margin, border, or padding. It should use only the style supplied by HTML or CSS.

For `blockquote`, keep default quote CSS on the `DivBlock` itself:

```css
blockquote {
    margin-top: 0.5em;
    margin-bottom: 0.5em;
    margin-left: 1em;
    border-left: 1px solid #888888;
    padding-left: 0.5em;
}
```

This default should be applied to the `DivBlock`, not to every child `TextBlock`.

If a `DivBlock` represents `blockquote`, its stored `ElementName` is `blockquote`, but the internal class remains `DivBlock` unless a thin `BlockQuoteBlock` subclass is used.

## Layout Design

`DivBlock::OnLayout()` should apply the container box once, then layout children.

Conceptual behavior:

```text
pos is the container border box

flow.Left += container margin-left
flow.CurY += container margin-top

flow.Left += container border-left + padding-left
flow.CurY += container border-top + padding-top

for each child:
    child->OnLayout(flow)
    update container pos.y2

flow.CurY += container margin-bottom + border-bottom + padding-bottom
flow.Left -= container margin-left + border-left + padding-left
```

Important:

- `DivBlock::GetPos()` should return the container border box.
- Child blocks can be `TextBlock`, `ListBlock`, `ImageBlock`, `HorzRuleBlock`, or another `DivBlock`.
- Child `TextBlock` styles should not duplicate the container `margin`, `border`, or `padding`.
- The `DivBlock` owns the box properties.

## Paint Design

`DivBlock::OnPaint()` should:

1. Handle selection before painting.
2. Draw the container background.
3. Draw container edges using `PaintContext::DrawBox()`.
4. Draw children.
5. Handle selection after painting.

Draw order:

```text
Margin
Border
Padding
Children
```

Border color comes from the container style. If no border color is available, use a neutral fallback, for example:

```text
LColour(180, 180, 180)
```

For blockquote, a left border is the main visual signal, but the implementation should remain generic. A `div` may have no border.

## Serialization Design

`DivBlock::ToHtml()` should serialize using the stored element name.

For full output:

```text
DivBlock div
  TextBlock child
```

should output:

```html
<div class='style_name'>
  <p>content</p>
</div>
```

For a blockquote:

```text
DivBlock blockquote
  TextBlock child
```

should output:

```html
<blockquote class='style_name'>
  <p>content</p>
</blockquote>
```

Conceptual implementation:

```cpp
const char *Tag = GetElementName();

if (Style && Style->Name)
    s.Print("<%s class='%s'>\n", Tag, Style->Name.Get());
else
    s.Print("<%s>\n", Tag);

for each child block:
    if range overlaps child:
        child->ToHtml(s, Media, ChildRange);

s.Print("</%s>\n", Tag);
```

For partial selection, the wrapper should still be emitted if the selection overlaps the container. If the selection does not overlap any child content but still represents the container itself, output an empty wrapper.

## Selection and Tree Resolution

Selection should know that `DivBlock` can contain children.

The existing top-level HTML selection resolves the root block and an offset within that root. For a `DivBlock`, the root block is the container, and the selection offset is interpreted relative to the container length.

`DivBlock::ToHtml()` must map container-relative ranges to child ranges.

If selection spans across a `DivBlock`, output:

```text
container wrapper
  only the overlapped child blocks
container end
```

This is similar to `ListBlock`, but the wrapper comes from `GetElementName()`.

## Editing Behavior

`DivBlock` should implement `Block` behavior by delegating to child blocks:

- `Length()`
- `GetLines()`
- `OffsetToLine()`
- `LineToOffset()`
- `HitTest()`
- `GetPosFromIndex()`
- `Seek()`
- `CopyAt()`
- `FindAt()`
- `DoContext()`
- `SetSpellingErrors()`
- `IncAllStyleRefs()`
- `DumpNodes()`
- `Clone()`

For editing methods:

- `AddText()`
- `DeleteAt()`
- `ChangeStyle()`
- `DoCase()`
- `Split()`

The container should usually delegate to an editable child leaf block.

Useful helper:

```text
DivBlock::GetTextBlock()
```

This should ensure at least one child `TextBlock` exists, similar to `ListBlock::GetTextBlock()`.

## Cursor Behavior

Cursor should be able to enter a `DivBlock`.

Recommended behavior:

- If the container has a `TextBlock`, cursor lands inside the child `TextBlock`.
- If the user clicks on the container border area, hit-test should resolve to the first editable child block.
- If the container is empty, ensure at least one placeholder `TextBlock` exists.

Using a guaranteed editable child is simpler than allowing cursor directly on the container node.

## Splitting and Merging

Container behavior should be conservative:

- If the cursor is inside a child `TextBlock`, split that child, not the container.
- If the container becomes empty after deletion, remove the container.
- Do not automatically merge containers unless the resulting structure is unambiguous.
- If pasting a container into a container, preserve the source structure.

Example:

```text
DivBlock blockquote
  TextBlock: first

TextBlock: second
```

If `first` and `second` are merged across the container boundary, the `blockquote` wrapper is lost. That should not happen automatically unless the user explicitly removes the quote.

## Context Menu and Formatting

Formatting should be separated into two concepts:

1. Container styling:
   - applies to `DivBlock`
   - uses `margin`, `border`, `padding` (inherited from `LCssBox<LRect>`)

2. Text styling:
   - applies to child `TextBlock`
   - uses font, color, bold, italic, etc.

`DivBlock::SetStyle()` should apply the container style.

`ChangeStyle()` should delegate character-level style changes to child blocks.

## Current Blockquote Behavior

The current Rte2 parser recognizes `TAG_BLOCKQUOTE` in:

```text
lgi/trunk/src/common/Widgets/richText2/RichTextEditPriv.cpp
```

But it only adds CSS properties to the current style. It does not create a tree node.

That produces effectively:

```text
TextBlock with margin-left
```

It can visually indent text, but it cannot reliably round-trip as:

```html
<blockquote>...</blockquote>
```

The `DivBlock` design fixes this by creating a real container node and storing the element name.

## Documentation Reconciliation

Existing Rte2 documentation says blocks have no hierarchy. That statement is not fully correct once `DivBlock` and `ListBlock` are considered.

Both are container blocks with child blocks:

```text
DivBlock
  children

ListBlock
  children
```

The documentation should describe blocks as mostly leaf text blocks, but with explicit container exceptions.

## Recommended Implementation Checklist

1. Add `DivBlock` as the generic concrete `Block` subclass.
2. Store `TagId` and `ElementName` in `DivBlock`.
3. `GetClass()` returns `"DivBlock"`.
4. Add `GetElementName()` returning the stored HTML tag name.
5. Default `ElementName` is `"div"`.
6. For `TAG_BLOCKQUOTE`, create `DivBlock` with `ElementName = "blockquote"`.
7. Add parser special case for `TAG_DIV` and `TAG_BLOCKQUOTE`.
8. Make sure child styles do not duplicate the container margin, border, or padding.
9. Ensure empty containers can still be edited.
10. Implement `ToHtml()` using `GetElementName()`.
11. Verify selection HTML emits the correct wrapper.
12. Add debug dump/tree display support so container nodes are visible.
13. Add manual test cases for div and blockquote round-trip.
14. Optionally add a thin `BlockQuoteBlock : DivBlock` subclass if debug class names need to show `BlockQuoteBlock`.

## Test Cases

Basic div:

```html
<div class='styled'>content</div>
```

Expected tree:

```text
DivBlock div
  TextBlock: content
```

Expected HTML:

```html
<div class='styled'>
  <p>content</p>
</div>
```

Basic blockquote:

```html
<blockquote>quoted</blockquote>
```

Expected tree:

```text
DivBlock blockquote
  TextBlock: quoted
```

Expected HTML:

```html
<blockquote class='quote'>
  <p>quoted</p>
</blockquote>
```

Nested content:

```html
<blockquote>
  <p>first</p>
  <ul><li>second</li></ul>
</blockquote>
```

Expected tree:

```text
DivBlock blockquote
  TextBlock: first
  ListBlock
    TextBlock: second
```

Expected HTML should preserve the wrapper and nested list.

Nested container:

```html
<div>
  <blockquote>quoted</blockquote>
  <p>outside</p>
</div>
```

Expected tree:

```text
DivBlock div
  DivBlock blockquote
    TextBlock: quoted
  TextBlock: outside
```

Selection behavior:

- selecting inside a `DivBlock` should still include the container wrapper;
- selecting from inside a container to outside should output only the overlapped container portion;
- selecting a div should serialize as `<div>`, not `<blockquote>`;
- selecting a blockquote should serialize as `<blockquote>`.

## Final Decision

Use `DivBlock` as the generic container block.

A blockquote is a `DivBlock` whose stored HTML element name is `blockquote`.

If a specialized `BlockQuoteBlock` class is needed, it should be a thin wrapper or subclass that only sets the default element name and default CSS. It should not duplicate the generic container behavior.

The essential fields are:

```text
Style
TagId
ElementName
Margin
Border
Padding
Pos
blocks
parent
```

The essential serialization rule is:

```text
serialize using ElementName, not GetClass()
```

This gives a generic container design while still preserving `<blockquote>` round-tripping.
