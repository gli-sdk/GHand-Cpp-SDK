import xml.etree.ElementTree as ET
from datetime import datetime

def convert_xml_to_html(xml_file, html_file):
    tree = ET.parse(xml_file)
    root = tree.getroot()
    
    html = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <title>测试报告</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        .summary {{ background: #f0f0f0; padding: 15px; border-radius: 5px; }}
        .pass {{ color: green; }}
        .fail {{ color: red; }}
        table {{ border-collapse: collapse; width: 100%; margin-top: 20px; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background: #4CAF50; color: white; }}
        tr:nth-child(even) {{ background: #f2f2f2; }}
        .note {{ color: #666; font-style: italic; }}
    </style>
</head>
<body>
    <h1>测试报告</h1>
    <div class="summary">
        <p>总测试数：{root.get('tests')}</p>
        <p>失败数：{root.get('failures')}</p>
        <p>错误数：{root.get('errors')}</p>
        <p>执行时间：{root.get('time')}s</p>
        <p>生成时间：{root.get('timestamp')}</p>
    </div>
    <table>
        <tr><th>测试套件</th><th>测试用例</th><th>文件</th><th>行号</th><th>状态</th><th>耗时</th><th>用例描述</th></tr>
"""
    
    for testsuite in root.findall('testsuite'):
        for testcase in testsuite.findall('testcase'):
            status_class = 'pass' if testcase.get('result') == 'completed' else 'fail'
            # 获取注释，优先使用 note 属性，其次使用 description 属性
            note = testcase.get('note') or testcase.get('description') or '-'
            html += f"""<tr>
                <td>{testsuite.get('name')}</td>
                <td>{testcase.get('name')}</td>
                <td>{testcase.get('file')}</td>
                <td>{testcase.get('line')}</td>
                <td class="{status_class}">{testcase.get('result')}</td>
                <td>{testcase.get('time')}s</td>
                <td class="note">{note}</td>
            </tr>
"""
    
    html += """</table></body></html>"""
    
    with open(html_file, 'w', encoding='utf-8') as f:
        f.write(html)

if __name__ == '__main__':
    convert_xml_to_html('E:/work/xiaoyao-sdk-cpp/build/test_report.xml', 'test_report.html')
    print(f"测试报告已生成：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")