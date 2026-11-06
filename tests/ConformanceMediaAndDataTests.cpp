#include "ConformanceTestHarness.hpp"

bool TestGeometryFlatten() {
    Result<Ref<PathGeometry>> path = MakeRef<PathGeometry>();
    Result<Ref<PathFigure>> figure = MakeRef<PathFigure>();
    Result<Ref<LineSegment>> line = MakeRef<LineSegment>();
    Result<Ref<BezierSegment>> cubic = MakeRef<BezierSegment>();
    Result<Ref<QuadraticBezierSegment>> quad = MakeRef<QuadraticBezierSegment>();
    Result<Ref<ArcSegment>> arc = MakeRef<ArcSegment>();
    Result<Ref<PolyLineSegment>> polyLine = MakeRef<PolyLineSegment>();
    Result<Ref<PolyBezierSegment>> polyBezier = MakeRef<PolyBezierSegment>();
    Result<Ref<PolyQuadraticBezierSegment>> polyQuad =
        MakeRef<PolyQuadraticBezierSegment>();
    CHECK(path && figure && line && cubic && quad && arc && polyLine &&
        polyBezier && polyQuad);

    figure->SetStartPoint({0.0, 0.0});
    line->SetPoint({10.0, 0.0});
    figure->AddSegment(line.Value());
    cubic->SetPoint1({10.0, 10.0});
    cubic->SetPoint2({20.0, 10.0});
    cubic->SetPoint3({20.0, 0.0});
    figure->AddSegment(cubic.Value());
    quad->SetPoint1({30.0, 10.0});
    quad->SetPoint2({30.0, 0.0});
    figure->AddSegment(quad.Value());
    arc->SetPoint({40.0, 0.0});
    arc->SetSize({8.0, 8.0});
    arc->SetSweepDirection(Aero::Media::SweepDirection::Clockwise);
    figure->AddSegment(arc.Value());
    const Point polyPoints[] = {{50.0, 0.0}, {60.0, 0.0}};
    polyLine->SetPoints({polyPoints, 2U});
    figure->AddSegment(polyLine.Value());
    const Point bezierPts[] = {
        {70.0, 10.0}, {80.0, 10.0}, {80.0, 0.0}};
    polyBezier->SetPoints({bezierPts, 3U});
    figure->AddSegment(polyBezier.Value());
    const Point quadPts[] = {{90.0, 10.0}, {90.0, 0.0}};
    polyQuad->SetPoints({quadPts, 2U});
    figure->AddSegment(polyQuad.Value());
    path->AddFigure(figure.Value());

    PointSink sink;
    path->Flatten(sink);
    CHECK(sink.begins == 1U);
    CHECK(sink.ends == 1U);
    CHECK(sink.points.Size() > 8U);
    CHECK(Near(sink.points[0].x, 0.0, 0.01));
    CHECK(Near(sink.points[sink.points.Size() - 1U].x, 90.0, 1.5));

    LineGeometry transformed;
    transformed.SetStartPoint({0.0, 0.0});
    transformed.SetEndPoint({1.0, 0.0});
    Result<Ref<TranslateTransform>> offset = MakeRef<TranslateTransform>();
    CHECK(offset);
    offset->SetX(5.0);
    transformed.SetTransform(offset.Value());
    PointSink shifted;
    transformed.Flatten(shifted);
    CHECK(true);
    CHECK(shifted.points.Size() >= 2U);
    CHECK(Near(shifted.points[0].x, 5.0, 0.01));
    CHECK(Near(shifted.points[shifted.points.Size() - 1U].x, 6.0, 0.01));

    Result<Ref<LineGeometry>> a = MakeRef<LineGeometry>();
    Result<Ref<LineGeometry>> b = MakeRef<LineGeometry>();
    CHECK(a && b);
    a->SetStartPoint({0.0, 0.0});
    a->SetEndPoint({4.0, 0.0});
    b->SetStartPoint({0.0, 2.0});
    b->SetEndPoint({4.0, 2.0});
    Result<Ref<GeometryGroup>> group = MakeRef<GeometryGroup>();
    CHECK(group);
    group->Add(a.Value());
    group->Add(b.Value());
    PointSink grouped;
    group->Flatten(grouped);
    CHECK(grouped.points.Size() >= 4U);

    Result<Ref<CombinedGeometry>> combined = MakeRef<CombinedGeometry>();
    CHECK(combined);
    combined->SetGeometry1(a.Value());
    combined->SetGeometry2(b.Value());
    combined->SetGeometryCombineMode(GeometryCombineMode::Union);
    PointSink unioned;
    combined->Flatten(unioned);
    CHECK(unioned.points.Size() >= grouped.points.Size());
    combined->SetGeometryCombineMode(GeometryCombineMode::Exclude);
    PointSink excluded;
    combined->Flatten(excluded);
    CHECK(excluded.points.Size() >= 2U);
    CHECK(excluded.points.Size() < unioned.points.Size());
    return true;
}

bool SameFlattened(const PointSink& left, const PointSink& right) noexcept {
    if (left.begins != right.begins || left.ends != right.ends ||
        left.points.Size() != right.points.Size()) {
        return false;
    }
    for (std::uint32_t index = 0U; index < left.points.Size(); ++index) {
        if (!Near(left.points[index].x, right.points[index].x, 0.05) ||
            !Near(left.points[index].y, right.points[index].y, 0.05)) {
            return false;
        }
    }
    return true;
}

bool TestClosedPathRelativeMove() {
    StreamGeometry ring;
    ring.SetData("M10,10 L30,10 L30,30 L10,30 Z m2,2 h16 v16 h-16 Z");
    PointSink sink;
    ring.Flatten(sink);
    CHECK(true);
    CHECK(sink.begins == 2U);
    CHECK(sink.figureStarts.Size() == 2U);
    CHECK(Near(sink.figureStarts[0].x, 10.0));
    CHECK(Near(sink.figureStarts[0].y, 10.0));
    CHECK(Near(sink.figureStarts[1].x, 12.0));
    CHECK(Near(sink.figureStarts[1].y, 12.0));
    const Aero::Rect ringBounds = ring.GetBounds();
    CHECK(ringBounds.x >= 9.9);
    CHECK(ringBounds.y >= 9.9);
    CHECK(ringBounds.width <= 20.1);
    CHECK(ringBounds.height <= 20.1);

    StreamGeometry innerFrame;
    innerFrame.SetData(
        "M489 658.92H38.65l-.17-.78a35.52 35.52 0 0 0-26.7-26.7l-.78-.18V38.66"
        "l.78-.18a35.53 35.53 0 0 0 26.7-26.7l.17-.78H489l.17.78a35.53 35.53 "
        "0 0 0 26.7 26.7l.78.18v592.6l-.78.18a35.52 35.52 0 0 0-26.7 26.7Zm-448.79-2"
        "h447.2a37.51 37.51 0 0 1 27.24-27.25V40.25A37.51 37.51 0 0 1 487.45 13"
        "H40.25A37.54 37.54 0 0 1 13 40.25V629.67A37.53 37.53 0 0 1 40.25 656.92Z");
    PointSink frameSink;
    innerFrame.Flatten(frameSink);
    CHECK(true);
    CHECK(frameSink.begins == 2U);
    const Aero::Rect frameBounds = innerFrame.GetBounds();
    CHECK(frameBounds.x > -1.0);
    CHECK(frameBounds.y > -1.0);
    CHECK(frameBounds.width < 540.0);
    CHECK(frameBounds.height < 680.0);

    StreamGeometry listItem;
    listItem.SetData(
        "M335.67 47.74H29.81l-20-20 20-20H335.67l20 20Zm-305-2h304.2l18-18-18-18"
        "H30.64l-18 18Z");
    PointSink itemSink;
    listItem.Flatten(itemSink);
    CHECK(true);
    CHECK(itemSink.begins == 2U);
    const Aero::Rect itemBounds = listItem.GetBounds();
    CHECK(itemBounds.x > -1.0);
    CHECK(itemBounds.width < 380.0);
    return true;
}

bool TestStreamGeometryFlattenCore() {
    StreamGeometry stream;
    stream.SetData("M 0,0 L 10,0 C 10,10 20,10 20,0 Q 30,10 30,0 A 8,8 0 0 1 40,0 Z");
    PointSink fromData;
    stream.Flatten(fromData);
    CHECK(true);
    CHECK(fromData.begins >= 1U);
    CHECK(fromData.points.Size() > 8U);
    CHECK(stream.GetBounds().width > 0.0);

    Result<Ref<PathGeometry>> pathGeometry = MakeRef<PathGeometry>();
    Result<Ref<PathFigure>> figure = MakeRef<PathFigure>();
    Result<Ref<LineSegment>> first = MakeRef<LineSegment>();
    Result<Ref<LineSegment>> second = MakeRef<LineSegment>();
    CHECK(pathGeometry && figure && first && second);
    figure->SetStartPoint({0.0, 0.0});
    figure->SetIsClosed(true);
    first->SetPoint({10.0, 0.0});
    second->SetPoint({10.0, 10.0});
    figure->AddSegment(first.Value());
    figure->AddSegment(second.Value());
    pathGeometry->AddFigure(figure.Value());
    StreamGeometry lineStream;
    lineStream.SetData("M 0,0 L 10,0 L 10,10 Z");
    PointSink fromStream;
    PointSink fromPathGeometry;
    lineStream.Flatten(fromStream);
    CHECK(true);
    pathGeometry->Flatten(fromPathGeometry);
    CHECK(SameFlattened(fromStream, fromPathGeometry));

    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    Result<Ref<Path>> path = MakeRef<Path>();
    Result<Ref<StreamGeometry>> mounted = MakeRef<StreamGeometry>();
    CHECK(path && mounted);
    mounted->SetData(stream.GetData());
    path->SetData(mounted.Value());
    CHECK(live->view->SetContent(path.Value(), {200.0, 200.0}));
    Pump(*live->view, 0.016);
    CHECK(path->GetGeometryBounds().width > 0.0);
    return true;
}

bool TestStreamGeometryContextFlatten() {
    const StringView data(
        "M 0,0 L 10,0 C 10,10 20,10 20,0 Q 30,10 30,0");
    StreamGeometry parsed;
    parsed.SetData(data);
    PointSink expected;
    parsed.Flatten(expected);
    CHECK(true);
    StreamGeometry built;
    StreamGeometryContext context = built.Open();
    context.BeginFigure({0.0, 0.0}, true, false);
    context.LineTo({10.0, 0.0}, true, false);
    context.BezierTo({10.0, 10.0}, {20.0, 10.0}, {20.0, 0.0}, true, false);
    context.QuadraticBezierTo({30.0, 10.0}, {30.0, 0.0}, true, false);
    context.Close();
    PointSink actual;
    built.Flatten(actual);
    CHECK(true);
    CHECK(SameFlattened(expected, actual));
    return true;
}

bool TestTimelineDurationAndKeyTime() {
    Duration automatic = Duration::Automatic();
    CHECK(automatic.IsAutomatic());
    Duration forever = Duration::Forever();
    CHECK(forever.IsForever());
    Result<Duration> parsedAuto = Duration::TryParse("Automatic");
    Result<Duration> parsedForever = Duration::TryParse("Forever");
    CHECK(parsedAuto && parsedAuto.Value().IsAutomatic());
    CHECK(parsedForever && parsedForever.Value().IsForever());

    CHECK(KeyTime::Uniform().IsUniform());
    CHECK(KeyTime::Paced().IsPaced());
    Result<KeyTime> percent = KeyTime::TryParse("50%");
    CHECK(percent && percent.Value().IsPercent());
    CHECK(Near(percent.Value().GetPercent(), 0.5));
    const std::uint64_t uniform =
        KeyTime::Uniform().ResolveMicroseconds(1000U, 0U, 4U);
    const std::uint64_t paced =
        KeyTime::Paced().ResolveMicroseconds(1000U, 0U, 4U);
    CHECK(uniform == 250U);
    CHECK(paced == uniform);
    CHECK(percent.Value().ResolveMicroseconds(1000U, 0U, 4U) == 500U);

    DoubleAnimation animation;
    animation.SetDuration(Duration::Automatic());
    CHECK(animation.GetDuration().IsAutomatic());
    animation.SetDuration(Duration::Forever());
    CHECK(animation.GetDuration().IsForever());
    Style style(DoubleAnimation::StaticTypeId());
    {
        Setter duration;
        duration.Set(Timeline::DurationProperty, Duration::Forever());
        style.AddSetter(duration);
        Setter repeat;
        repeat.Set(Timeline::RepeatBehaviorProperty, RepeatBehavior::Forever());
        style.AddSetter(repeat);
    }
    return true;
}

bool TestCollectionViewAndVirtualization() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& host = *live->view;
    host.SetSize({160.0, 72.0});

    Result<Ref<Button>> narrow = MakeRef<Button>();
    Result<Ref<Button>> wide = MakeRef<Button>();
    Result<Ref<Button>> wider = MakeRef<Button>();
    CHECK(narrow && wide && wider);
    narrow->SetWidth(10.0);
    wide->SetWidth(80.0);
    wider->SetWidth(90.0);

    ObservableObjectCollection source;
    source.Add(narrow.Value());
    source.Add(wide.Value());
    source.Add(wider.Value());

    CollectionView view(&source);
    CHECK(view.GetCount() == 3U);
    view.SetFilter(&KeepWide);
    CHECK(view.GetCount() == 2U);
    CHECK(view.GetItem(0U).Get() == wide.Value().Get());
    view.SortBy("Width", ListSortDirection::Descending);
    CHECK(view.GetItem(0U).Get() == wider.Value().Get());
    CHECK(view.GetItem(1U).Get() == wide.Value().Get());
    CHECK(view.MoveCurrentTo(wide.Value().Get()));
    CHECK(view.GetCurrentItem().Get() == wide.Value().Get());

    {
        Result<Ref<Person>> p1 = MakeRef<Person>();
        Result<Ref<Person>> p2 = MakeRef<Person>();
        CHECK(p1 && p2);
        String n1, n2;
        CHECK(n1.Assign("Bob"));
        CHECK(n2.Assign("Alice"));
        p1.Value()->SetName(std::move(n1));
        p2.Value()->SetName(std::move(n2));
        ObservableObjectCollection personSource;
        personSource.Add(p1.Value());
        personSource.Add(p2.Value());
        CollectionView personView(&personSource);
        personView.SortBy("Name", ListSortDirection::Ascending);
        CHECK(personView.GetItem(0U).Get() == p2.Value().Get());
        CHECK(personView.GetItem(1U).Get() == p1.Value().Get());
        personView.SortBy("Name", ListSortDirection::Descending);
        CHECK(personView.GetItem(0U).Get() == p1.Value().Get());
        CHECK(personView.GetItem(1U).Get() == p2.Value().Get());
    }

    Result<Ref<ObservableObjectCollection>> items =
        MakeRef<ObservableObjectCollection>();
    CHECK(items);
    Vector<Ref<TextBlock>> rows;
    for (std::uint32_t index = 0U; index < 40U; ++index) {
        Result<Ref<TextBlock>> row = MakeRef<TextBlock>();
        CHECK(row);
        row->SetHeight(24.0);
        items->Add(row.Value());
        rows.PushBack(row.Value());
    }

    Result<Ref<ListBox>> list = MakeRef<ListBox>();
    CHECK(list);
    list->SetItemsSource(items.Value());
    CollectionView* defaultView =
        CollectionViewSource::GetDefaultView(items.Value().Get());
    CHECK(defaultView != nullptr);
    CHECK(defaultView->GetCount() == 40U);
    list->SetIsSynchronizedWithCurrentItem(false);
    list->SetSelectedItem(rows[3]);
    CHECK(list->GetSelectedItem().Get() == rows[3].Get());
    defaultView->MoveCurrentTo(rows[7].Get());
    CHECK(list->GetSelectedItem().Get() == rows[3].Get());
    CHECK(defaultView->GetCurrentItem().Get() == rows[7].Get());
    CHECK(list->GetIndexOfItem(rows[3].Get()) == 3U);

    defaultView->SetFilter(
        [](const Aero::Base::Object* item) noexcept {
            return item != nullptr;
        });
    CHECK(list->GetIndexOfItem(rows[3].Get()) != UINT32_MAX);
    defaultView->SetFilter(
        [](const Aero::Base::Object*) noexcept { return false; });
    CHECK(list->GetIndexOfItem(rows[3].Get()) == UINT32_MAX);
    defaultView->SetFilter({});
    CHECK(defaultView->GetCount() == 40U);

    CHECK(host.SetContent(list.Value()));
    Pump(host, 0.016);
    static_cast<void>(list->ApplyTemplate());
    Pump(host, 0.032);

    Panel* itemsHost = list->GetItemsHost();
    if (VirtualizingStackPanel* virtualizing =
            TryCast<VirtualizingStackPanel>(
                static_cast<Aero::Base::Object*>(itemsHost))) {
        virtualizing->SetVerticalOffset(240.0);
        Pump(host, 0.048);
        const std::uint32_t first =
            virtualizing->GetRealizedFirstIndex();
        Aero::Controls::ItemContainerGenerator* generator =
            list->GetItemContainerGenerator();
        CHECK(generator != nullptr);
        CHECK(generator->GetFirstGeneratedIndex() == first);
        const std::uint32_t target = first + 1U;
        CHECK(list->Select(target));
        Pump(host, 0.064);
        FrameworkElement* container = generator->ContainerFromIndex(target);
        CHECK(container != nullptr);
        ListBoxItem* item = TryCast<ListBoxItem>(container);
        CHECK(item != nullptr);
        CHECK(item->GetIsSelected());
        CHECK(list->GetIsSelected(target));
    } else {
        CHECK(list->Select(12U));
        Pump(host, 0.048);
        Aero::Controls::ItemContainerGenerator* generator =
            list->GetItemContainerGenerator();
        if (generator != nullptr && generator->GetGeneratedCount() > 12U) {
            FrameworkElement* container = generator->ContainerFromIndex(12U);
            if (ListBoxItem* item = TryCast<ListBoxItem>(container)) {
                CHECK(item->GetIsSelected());
            }
        }
    }
    return true;
}

bool TestTemplateResolveOrder() {
    Result<Ref<ItemsControl>> host = MakeRef<ItemsControl>();
    Result<Ref<DataTemplate>> selectorTemplate = MakeRef<DataTemplate>();
    Result<Ref<DataTemplate>> itemTemplate = MakeRef<DataTemplate>();
    Result<Ref<DataTemplate>> implicitTemplate = MakeRef<DataTemplate>();
    Result<Ref<Button>> item = MakeRef<Button>();
    CHECK(host && selectorTemplate && itemTemplate && implicitTemplate && item);

    implicitTemplate->SetDataType(Button::StaticTypeId());
    host->GetResources().Add(
        implicitTemplate->GetImplicitKey(),
        Aero::Value::FromObject(
            DataTemplate::StaticTypeId(), implicitTemplate.Value()));

    Ref<DataTemplate> resolved = host->ResolveItemTemplate(item.Value(), 0U);
    CHECK(resolved.Get() == implicitTemplate.Value().Get());

    host->SetItemTemplate(itemTemplate.Value());
    resolved = host->ResolveItemTemplate(item.Value(), 0U);
    CHECK(resolved.Get() == itemTemplate.Value().Get());

    Result<Ref<PickingSelector>> selector = MakeRef<PickingSelector>();
    CHECK(selector);
    selector->pick = selectorTemplate.Value();
    host->SetItemTemplateSelector(selector.Value());
    resolved = host->ResolveItemTemplate(item.Value(), 0U);
    CHECK(resolved.Get() == selectorTemplate.Value().Get());

    selector->enabled = false;
    resolved = host->ResolveItemTemplate(item.Value(), 0U);
    CHECK(resolved.Get() == itemTemplate.Value().Get());
    return true;
}

bool TestStrokeJoinCapFillRule() {
    Path path;
    path.SetStrokeLineJoin(PenLineJoin::Round);
    path.SetStrokeStartLineCap(PenLineCap::Triangle);
    path.SetStrokeEndLineCap(PenLineCap::Square);
    path.SetFillRule(FillRule::EvenOdd);
    CHECK(path.GetStrokeLineJoin() == PenLineJoin::Round);
    CHECK(path.GetStrokeStartLineCap() == PenLineCap::Triangle);
    CHECK(path.GetStrokeEndLineCap() == PenLineCap::Square);
    CHECK(path.GetFillRule() == FillRule::EvenOdd);
    path.SetFillRule(FillRule::Nonzero);
    CHECK(path.GetFillRule() == FillRule::Nonzero);

    Result<Ref<Pen>> pen = MakeRef<Pen>();
    Result<Ref<Aero::Media::Brush>> stroke = Aero::Media::MakeSolidColorBrush(
        {0.0F, 0.0F, 0.0F, 1.0F});
    CHECK(pen && stroke);
    pen->SetBrush(stroke.Value());
    pen->SetThickness(4.0);
    pen->SetLineJoin(PenLineJoin::Miter);
    pen->SetMiterLimit(1.0);
    pen->SetStartLineCap(PenLineCap::Square);
    pen->SetEndLineCap(PenLineCap::Triangle);
    path.SetPen(pen.Value());
    CHECK(path.GetPen().Get() == pen.Value().Get());
    CHECK(Near(path.GetPen()->GetMiterLimit(), 1.0, 0.001));
    Result<Ref<StreamGeometry>> geometry = MakeRef<StreamGeometry>();
    CHECK(geometry);
    geometry->SetData("M 0,0 L 20,0 L 10,20 Z");
    path.SetData(geometry.Value());
    path.SetStroke(stroke.Value());

    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    Result<Ref<Path>> mounted = MakeRef<Path>();
    CHECK(mounted);
    mounted->SetPen(pen.Value());
    mounted->SetData(geometry.Value());
    mounted->SetStroke(stroke.Value());
    mounted->SetStrokeLineJoin(PenLineJoin::Miter);
    CHECK(live->view->SetContent(mounted.Value(), {100.0, 100.0}));
    Pump(*live->view, 0.016);
    CHECK(mounted->GetGeometryBounds().width > 0.0);
    CHECK(mounted->GetGeometryBounds().height > 0.0);
    CHECK(Near(mounted->GetPen()->GetMiterLimit(), 1.0, 0.001));
    return true;
}

bool TestNotifyPropertyChangedBindLoop() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({200.0, 40.0});

    Result<Ref<Person>> person = MakeRef<Person>();
    CHECK(person);
    String name;
    CHECK(name.Assign("Ada"));
    person->SetName(std::move(name));

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(
        StringView("<TextBlock xmlns=\"urn:aero\" Text=\"{Binding Name}\"/>"));
    CHECK(document);
    TextBlock* label = document.Value().Root<TextBlock>();
    CHECK(label != nullptr);
    label->SetValue(
        FrameworkElement::DataContextProperty,
        Aero::Value::FromObject(Person::StaticTypeId(), person.Value()));
    CHECK(view.SetContent(
        std::move(document).Value(), {200.0, 40.0}));
    label = TryCast<TextBlock>(view.GetContent());
    CHECK(label != nullptr);
    Pump(view, 0.016);
    Pump(view, 0.032);
    Pump(view, 0.048);

    bool polled = false;
    for (std::uint32_t index = 0U; index < live->diagnostics.Size(); ++index) {
        if (Contains(
                live->diagnostics.Items()[index].Message(),
                "polling metadata")) {
            polled = true;
        }
    }
    CHECK(!polled);
    CHECK(label->GetText() == StringView("Ada"));

    String renamed;
    CHECK(renamed.Assign("Bob"));
    person->SetName(std::move(renamed));
    Pump(view, 0.064);
    CHECK(label->GetText() == StringView("Bob"));
    return true;
}

bool TestBindingExpressionAndLostFocus() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 80.0});

    Result<Ref<Person>> person = MakeRef<Person>();
    CHECK(person);
    String name;
    CHECK(name.Assign("Ada"));
    person->SetName(std::move(name));

    Aero::Markup::XamlReader reader(live->gui);
    Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
        "<StackPanel xmlns=\"urn:aero\">"
        "<TextBox Text=\"{Binding Name, Mode=TwoWay}\"/>"
        "<TextBox Text=\"other\"/>"
        "</StackPanel>"));
    CHECK(document);
    CHECK(view.SetContent(std::move(document).Value(), {240.0, 80.0}));
    Pump(view, 0.016);
    auto* root = TryCast<StackPanel>(view.GetContent());
    CHECK(root != nullptr);
    CHECK(root->GetChildren().GetCount() >= 2U);
    auto* input = TryCast<TextBox>(root->GetChildren().GetItem(0U));
    auto* other = TryCast<TextBox>(root->GetChildren().GetItem(1U));
    CHECK(input != nullptr && other != nullptr);
    input->SetValue(
        FrameworkElement::DataContextProperty,
        Aero::Value::FromObject(Person::StaticTypeId(), person.Value()));
    Pump(view, 0.032);
    Pump(view, 0.048);
    CHECK(input->GetText() == StringView("Ada"));

    BindingExpression expression = BindingOperations::GetBindingExpression(
        input, TextBox::TextProperty);
    CHECK(expression.IsValid());
    CHECK(expression.Status() == BindingStatus::Active);

    input->SetText("Bob");
    Pump(view, 0.064);
    CHECK(person->GetName() == StringView("Ada"));

    CHECK(input->Focus());
    Pump(view, 0.080);
    CHECK(other->Focus());
    Pump(view, 0.096);
    CHECK(person->GetName() == StringView("Bob"));

    BindingExpression invalid;
    CHECK(!invalid.IsValid());
    CHECK(invalid.Status() == BindingStatus::Unattached);
    CHECK(!invalid.UpdateSource().IsOk());
    CHECK(!invalid.UpdateTarget().IsOk());

    Result<Aero::Markup::XamlDocument> explicitDocument = reader.Parse(StringView(
        "<TextBox xmlns=\"urn:aero\""
        " Text=\"{Binding Name, Mode=TwoWay, UpdateSourceTrigger=Explicit}\"/>"));
    CHECK(explicitDocument);
    CHECK(view.SetContent(std::move(explicitDocument).Value(), {240.0, 40.0}));
    auto* explicitBox = TryCast<TextBox>(view.GetContent());
    CHECK(explicitBox != nullptr);
    explicitBox->SetValue(
        FrameworkElement::DataContextProperty,
        Aero::Value::FromObject(Person::StaticTypeId(), person.Value()));
    Pump(view, 0.112);
    CHECK(explicitBox->GetText() == StringView("Bob"));
    BindingExpression explicitExpression =
        BindingOperations::GetBindingExpression(
            explicitBox, TextBox::TextProperty);
    CHECK(explicitExpression.IsValid());
    explicitBox->SetText("Cara");
    Pump(view, 0.128);
    CHECK(person->GetName() == StringView("Bob"));
    CHECK(explicitExpression.UpdateSource().IsOk());
    CHECK(person->GetName() == StringView("Cara"));
    String restored;
    CHECK(restored.Assign("Ada"));
    person->SetName(std::move(restored));
    CHECK(explicitExpression.UpdateTarget().IsOk());
    CHECK(explicitBox->GetText() == StringView("Ada"));
    return true;
}

bool TestCustomItemsSourceThunk() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);

    Result<Ref<UnregisteredItems>> raw = MakeRef<UnregisteredItems>();
    CHECK(raw);
    Result<Ref<ListBox>> emptyBox = MakeRef<ListBox>();
    CHECK(emptyBox);
    emptyBox->SetItemsSource(raw.Value());
    CHECK(emptyBox->GetCount() == 0U);
    CHECK(TryCastToInterface<IItemsSource>(raw.Value().Get()) == nullptr);

    Result<Ref<NamedItems>> named = MakeRef<NamedItems>();
    CHECK(named);
    Result<Ref<TextBlock>> one = MakeRef<TextBlock>();
    Result<Ref<TextBlock>> two = MakeRef<TextBlock>();
    CHECK(one && two);
    named->Add(one.Value());
    named->Add(two.Value());
    CHECK(TryCastToInterface<IItemsSource>(named.Value().Get()) != nullptr);

    Result<Ref<ListBox>> box = MakeRef<ListBox>();
    CHECK(box);
    box->SetItemsSource(named.Value());
    CHECK(box->GetCount() == 2U);
    CHECK(box->GetItem(1U).Get() == two.Value().Get());

    Result<Ref<ObservableCollection<BindingSlotItem>>> typed =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    CHECK(typed);
    Result<Ref<BindingSlotItem>> first = MakeRef<BindingSlotItem>();
    Result<Ref<BindingSlotItem>> second = MakeRef<BindingSlotItem>();
    CHECK(first && second);
    typed->Add(first.Value());
    typed->Add(second.Value());
    CHECK(typed->RuntimeType() ==
        ObservableCollectionBase::StaticTypeId());
    CHECK(TryCastToInterface<IItemsSource>(typed.Value().Get()) != nullptr);
    CHECK(Aero::Collections::CollectionAsItemsSource(typed.Value().Get()) !=
        nullptr);

    Result<Ref<ListBox>> typedBox = MakeRef<ListBox>();
    CHECK(typedBox);
    typedBox->SetItemsSource(typed.Value());
    CHECK(typedBox->GetCount() == 2U);
    CHECK(typedBox->GetItem(0U).Get() == first.Value().Get());
    return true;
}

Result<Ref<BindingItemsViewModel>> MakeBindingItemsViewModel(
    std::uint32_t count) noexcept {
    Result<Ref<BindingItemsViewModel>> model =
        MakeRef<BindingItemsViewModel>();
    if (!model) return model;
    Result<Ref<ObservableCollection<BindingSlotItem>>> items =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    if (!items) return items.GetStatus();
    Result<Ref<BindingPlayer>> player = MakeRef<BindingPlayer>();
    if (!player) return player.GetStatus();
    Result<Ref<ObservableCollection<BindingSlotItem>>> slots =
        MakeRef<ObservableCollection<BindingSlotItem>>();
    if (!slots) return slots.GetStatus();
    for (std::uint32_t index = 0U; index < count; ++index) {
        Result<Ref<BindingSlotItem>> item = MakeRef<BindingSlotItem>();
        if (!item) return item.GetStatus();
        items->Add(item.Value());
        Result<Ref<BindingSlotItem>> slot = MakeRef<BindingSlotItem>();
        if (!slot) return slot.GetStatus();
        slots->Add(slot.Value());
    }
    model->SetItems(items.Value());
    player->SetSlots(slots.Value());
    model->SetPlayer(player.Value());
    return model;
}

void PumpBindings(LiveGui& live) noexcept {
    for (int i = 0; i < 16; ++i) {
        PumpForward(live);
    }
}

bool CountMatchesAfterLayout(
    LiveGui& live,
    ItemsControl& list,
    std::uint32_t expected) noexcept {
    std::fprintf(stderr, "[debug CountMatchesAfterLayout] 1. ApplyTemplate\n");
    static_cast<void>(list.ApplyTemplate());
    std::fprintf(stderr, "[debug CountMatchesAfterLayout] 2. PumpBindings\n");
    PumpBindings(live);
    std::fprintf(stderr, "[debug CountMatchesAfterLayout] 3. ApplyTemplate\n");
    static_cast<void>(list.ApplyTemplate());
    std::fprintf(stderr, "[debug CountMatchesAfterLayout] 4. PumpBindings\n");
    PumpBindings(live);
    std::fprintf(stderr, "[debug CountMatchesAfterLayout] 4. PumpBindings done\n");
    const std::uint32_t count = list.GetCount();
    const std::uint32_t realized = list.GetRealizedItemCount();
    std::fprintf(stderr, "[debug CountMatchesAfterLayout] 5. count=%u realized=%u\n", count, realized);
    if (count != expected) {
        std::fprintf(
            stderr,
            "ItemsControl GetCount=%u expected=%u realized=%u host=%d\n",
            count,
            expected,
            realized,
            list.GetItemsHost() != nullptr);
        return false;
    }
    if (expected > 0U &&
        list.GetItemsHost() != nullptr &&
        realized == 0U) {
        std::fprintf(
            stderr,
            "ItemsControl host present but realized 0 of %u\n",
            expected);
        return false;
    }
    return true;
}

bool TestClrItemsSourceBindingAfterDataContext() {
    LiveGui* live = NewLiveGui();
    CHECK(live != nullptr);
    View& view = *live->view;
    view.SetSize({240.0, 160.0});
    constexpr std::uint32_t kCount = 4U;
    Aero::Markup::XamlReader reader(live->gui);

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        CHECK(model->GetItems()->GetCount() == kCount);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " Width=\"240\" Height=\"160\">"
            "<ListBox x:Name=\"List\" ItemsSource=\"{Binding Items}\">"
            "<ListBox.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ListBox.ItemsPanel>"
            "</ListBox>"
            "</Grid>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        ListBox* list = grid->FindName<ListBox>(StringView("List"));
        CHECK(list != nullptr);
        CHECK(list->GetCount() == 0U);
        grid->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
        CHECK(list->GetDataContext().AsObject().Get() == model.Value().Get());
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " Width=\"240\" Height=\"160\">"
            "<ItemsControl x:Name=\"List\" ItemsSource=\"{Binding Items}\">"
            "<ItemsControl.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ItemsControl.ItemsPanel>"
            "</ItemsControl>"
            "</Grid>"));
        CHECK(document);
        Grid* grid = document.Value().Root<Grid>();
        CHECK(grid != nullptr);
        grid->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        ItemsControl* list = grid->FindName<ItemsControl>(StringView("List"));
        CHECK(list != nullptr);
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<ListBox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " x:Name=\"List\" Width=\"240\" Height=\"160\""
            " ItemsSource=\"{Binding Items}\">"
            "<ListBox.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ListBox.ItemsPanel>"
            "</ListBox>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        ListBox* list = TryCast<ListBox>(view.GetContent());
        CHECK(list != nullptr);
        list->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
    }

    {
        Result<Ref<ObservableCollection<BindingSlotItem>>> items =
            MakeRef<ObservableCollection<BindingSlotItem>>();
        CHECK(items);
        for (std::uint32_t index = 0U; index < kCount; ++index) {
            Result<Ref<BindingSlotItem>> item = MakeRef<BindingSlotItem>();
            CHECK(item);
            items->Add(item.Value());
        }
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<ListBox xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " x:Name=\"List\" Width=\"240\" Height=\"160\""
            " ItemsSource=\"{Binding}\">"
            "<ListBox.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ListBox.ItemsPanel>"
            "</ListBox>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        ListBox* list = TryCast<ListBox>(view.GetContent());
        CHECK(list != nullptr);
        list->SetDataContext(Ref<Aero::Base::Object>(items.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<ContentControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " Width=\"240\" Height=\"160\">"
            "<Viewbox>"
            "<ItemsControl x:Name=\"List\" ItemsSource=\"{Binding Items}\">"
            "<ItemsControl.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ItemsControl.ItemsPanel>"
            "</ItemsControl>"
            "</Viewbox>"
            "</ContentControl>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        ContentControl* host = TryCast<ContentControl>(view.GetContent());
        CHECK(host != nullptr);
        ItemsControl* list = host->FindName<ItemsControl>(StringView("List"));
        CHECK(list != nullptr);
        CHECK(list->GetCount() == 0U);
        host->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
        CHECK(list->GetDataContext().AsObject().Get() == model.Value().Get());
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " Width=\"240\" Height=\"160\">"
            "<ScrollViewer>"
            "<ItemsControl x:Name=\"List\" ItemsSource=\"{Binding Items}\">"
            "<ItemsControl.ItemsPanel>"
            "<ItemsPanelTemplate><StackPanel/></ItemsPanelTemplate>"
            "</ItemsControl.ItemsPanel>"
            "</ItemsControl>"
            "</ScrollViewer>"
            "</Grid>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        ItemsControl* list = grid->FindName<ItemsControl>(StringView("List"));
        CHECK(list != nullptr);
        grid->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(kCount);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<ContentControl xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " x:Name=\"Host\" Content=\"{Binding Player.Slots[0]}\"/>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 40.0}));
        ContentControl* host = TryCast<ContentControl>(view.GetContent());
        CHECK(host != nullptr);
        host->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        PumpBindings(*live);
        CHECK(host->GetContent().Kind() == Aero::Base::ValueKind::Object);
        CHECK(!host->GetContent().IsNullObject());
        CHECK(host->GetContent().AsObject().Get() ==
            model->GetPlayer()->GetSlots()->GetItem(0U).Get());
    }

    {
        Result<Ref<BindingItemsViewModel>> model =
            MakeBindingItemsViewModel(8U);
        CHECK(model);
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<StackPanel xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<ContentControl x:Name=\"Slot0\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[0]}\"/>"
            "<ContentControl x:Name=\"Slot1\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[1], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot2\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[2], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot3\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[3], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot4\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[4], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot5\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[5], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot6\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[6], Mode=TwoWay}\"/>"
            "<ContentControl x:Name=\"Slot7\" Content=\"{Binding}\" DataContext=\"{Binding Player.Slots[7], Mode=TwoWay}\"/>"
            "</StackPanel>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        auto* panel = TryCast<StackPanel>(view.GetContent());
        CHECK(panel != nullptr);
        panel->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        PumpBindings(*live);
        CHECK(panel->GetChildren().GetCount() == 8U);
        for (std::uint32_t index = 0U; index < 8U; ++index) {
            ContentControl* slot = TryCast<ContentControl>(
                panel->GetChildren().GetItem(index));
            CHECK(slot != nullptr);
            Aero::Base::Object* expected =
                model->GetPlayer()->GetSlots()->GetItem(index).Get();
            CHECK(slot->GetDataContext().Kind() == Aero::Base::ValueKind::Object);
            CHECK(slot->GetDataContext().AsObject().Get() == expected);
            CHECK(slot->GetContent().Kind() == Aero::Base::ValueKind::Object);
            CHECK(slot->GetContent().AsObject().Get() == expected);
        }
    }

    {
        Result<Ref<BindingDoItemsViewModel>> model =
            MakeRef<BindingDoItemsViewModel>();
        CHECK(model);
        Result<Ref<ObservableCollection<BindingSlotItem>>> items =
            MakeRef<ObservableCollection<BindingSlotItem>>();
        CHECK(items);
        Result<Ref<ObservableObjectCollection>> inventory =
            MakeRef<ObservableObjectCollection>();
        CHECK(inventory);
        for (std::uint32_t index = 0U; index < kCount; ++index) {
            Result<Ref<BindingSlotItem>> item = MakeRef<BindingSlotItem>();
            CHECK(item);
            items->Add(item.Value());
            inventory->Add(item.Value());
        }
        model->SetItems(items.Value());
        model->SetInventory(inventory.Value());
        Result<Aero::Markup::XamlDocument> document = reader.Parse(StringView(
            "<Grid xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\""
            " xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\""
            " Width=\"240\" Height=\"160\">"
            "<Grid.Resources>"
            "<ControlTemplate x:Key=\"SV\" TargetType=\"ScrollViewer\">"
            "<ScrollContentPresenter Content=\"{TemplateBinding Content}\"/>"
            "</ControlTemplate>"
            "</Grid.Resources>"
            "<ScrollViewer Template=\"{StaticResource SV}\">"
            "<ItemsControl x:Name=\"List\" ItemsSource=\"{Binding Inventory}\">"
            "<ItemsControl.ItemsPanel>"
            "<ItemsPanelTemplate><UniformGrid Columns=\"5\"/></ItemsPanelTemplate>"
            "</ItemsControl.ItemsPanel>"
            "<ItemsControl.ItemTemplate>"
            "<DataTemplate><ContentControl Content=\"{Binding}\"/></DataTemplate>"
            "</ItemsControl.ItemTemplate>"
            "</ItemsControl>"
            "</ScrollViewer>"
            "</Grid>"));
        CHECK(document);
        CHECK(view.SetContent(std::move(document).Value(), {240.0, 160.0}));
        Grid* grid = TryCast<Grid>(view.GetContent());
        CHECK(grid != nullptr);
        ItemsControl* list = grid->FindName<ItemsControl>(StringView("List"));
        CHECK(list != nullptr);
        CHECK(list->GetCount() == 0U);
        grid->SetDataContext(Ref<Aero::Base::Object>(model.Value()));
        CHECK(CountMatchesAfterLayout(*live, *list, kCount));
        CHECK(list->GetDataContext().AsObject().Get() == model.Value().Get());
    }

    {
        Result<Ref<ObservableObjectCollection>> named =
            MakeRef<ObservableObjectCollection>();
        CHECK(named);
        for (std::uint32_t index = 0U; index < kCount; ++index) {
            Result<Ref<BindingSlotItem>> item = MakeRef<BindingSlotItem>();
            CHECK(item);
            named->Add(item.Value());
        }
        Result<Ref<ListBox>> box = MakeRef<ListBox>();
        CHECK(box);
        box->SetItemsSource(named.Value());
        CHECK(box->GetCount() == kCount);
        CollectionView* defaultView =
            CollectionViewSource::GetDefaultView(named.Value().Get());
        CHECK(defaultView != nullptr);
        CHECK(defaultView->GetCount() == kCount);
    }
    return true;
}

