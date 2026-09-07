using System.Globalization;
using System.Windows;
using System.Windows.Controls;

namespace WpfPlugin;

/// <summary>
/// 벽 값을 받는 WPF 대화상자. XAML 없이 코드로 짜서 파일 하나로 끝낸다
/// (예제라 XAML 빌드 액션까지 설명하는 것보다 이쪽이 읽기 쉽다).
/// </summary>
internal sealed class WallWindow : Window
{
    private readonly TextBox _length = new() { Text = "2.0" };
    private readonly TextBox _thick = new() { Text = "0.2" };
    private readonly TextBox _height = new() { Text = "2.4" };

    public double WallLength { get; private set; }
    public double WallThick { get; private set; }
    public double WallHeight { get; private set; }

    public WallWindow(double length, double thick, double height)
    {
        Title = "벽 만들기 (WPF)";
        Width = 320;
        Height = 210;
        WindowStartupLocation = WindowStartupLocation.CenterScreen;
        ResizeMode = ResizeMode.NoResize;
        // 엔진 창이 뒤로 가지 않게 항상 위에 — 모달 부모를 지정할 수 없어서다
        // (엔진 창은 WPF 가 모르는 네이티브 창이다).
        Topmost = true;

        _length.Text = length.ToString(CultureInfo.InvariantCulture);
        _thick.Text = thick.ToString(CultureInfo.InvariantCulture);
        _height.Text = height.ToString(CultureInfo.InvariantCulture);

        var grid = new Grid { Margin = new Thickness(14) };
        for (int i = 0; i < 4; ++i) grid.RowDefinitions.Add(new RowDefinition());
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(70) });
        grid.ColumnDefinitions.Add(new ColumnDefinition());

        void Row(int r, string label, TextBox box)
        {
            var t = new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center };
            Grid.SetRow(t, r); Grid.SetColumn(t, 0); grid.Children.Add(t);
            box.Margin = new Thickness(0, 4, 0, 4);
            Grid.SetRow(box, r); Grid.SetColumn(box, 1); grid.Children.Add(box);
        }
        Row(0, "길이", _length);
        Row(1, "두께", _thick);
        Row(2, "높이", _height);

        var ok = new Button { Content = "확인", Width = 80, IsDefault = true, Margin = new Thickness(0, 0, 8, 0) };
        var cancel = new Button { Content = "취소", Width = 80, IsCancel = true };
        ok.Click += (_, _) =>
        {
            if (!TryRead()) return;   // 숫자가 아니면 닫지 않는다
            DialogResult = true;
        };
        var buttons = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Margin = new Thickness(0, 10, 0, 0),
        };
        buttons.Children.Add(ok);
        buttons.Children.Add(cancel);
        Grid.SetRow(buttons, 3); Grid.SetColumnSpan(buttons, 2); grid.Children.Add(buttons);

        Content = grid;
    }

    private bool TryRead()
    {
        // InvariantCulture — 지역 설정이 쉼표 소수점이어도 같은 규칙으로 읽는다.
        // ⚠️ && 로 이으면 뒤쪽 out 변수가 "확실히 대입됨"으로 인정되지 않는다(CS0165).
        //    세 번 다 시도한 뒤 결과를 합친다.
        bool okL = double.TryParse(_length.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out var l);
        bool okT = double.TryParse(_thick.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out var t);
        bool okH = double.TryParse(_height.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out var h);
        if (!okL || !okT || !okH || l <= 0 || t <= 0 || h <= 0)
        {
            MessageBox.Show("길이·두께·높이는 0보다 큰 숫자여야 합니다.", "벽 만들기",
                            MessageBoxButton.OK, MessageBoxImage.Warning);
            return false;
        }
        WallLength = l; WallThick = t; WallHeight = h;
        return true;
    }
}
