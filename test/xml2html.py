import xml.etree.ElementTree as ET
import os
from datetime import datetime

def parse_xml_report(file_path):
    """解析 XML 测试报告文件"""
    tree = ET.parse(file_path)
    root = tree.getroot()
    return root

def generate_html_report(root, output_path):
    """生成 HTML 格式的测试报告"""
    # 提取总体统计信息
    testsuites_info = root.attrib
    total_tests = testsuites_info.get('tests', '0')
    total_failures = testsuites_info.get('failures', '0')
    total_time = testsuites_info.get('time', '0')
    timestamp = testsuites_info.get('timestamp', '')

    # 构建 HTML 内容
    html_content = f"""
    <!DOCTYPE html>
    <html lang="zh-CN">
    <head>
        <meta charset="UTF-8">
        <title>测试报告</title>
        <style>
            body {{ font-family: Arial, sans-serif; margin: 20px; }}
            h1 {{ color: #333; }}
            .summary {{ background: #f9f9f9; padding: 15px; border-radius: 5px; margin-bottom: 20px; }}
            table {{ width: 100%; border-collapse: collapse; margin-top: 10px; }}
            th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
            th {{ background-color: #4CAF50; color: white; }}
            tr:nth-child(even) {{ background-color: #f2f2f2; }}
            .pass {{ color: green; font-weight: bold; }}
            .fail {{ color: red; font-weight: bold; }}
            .properties {{ font-size: 0.9em; color: #666; }}
        </style>
    </head>
    <body>
        <h1>测试报告汇总</h1>
        <div class="summary">
            <p><strong>生成时间:</strong> {timestamp}</p>
            <p><strong>总测试数:</strong> {total_tests} | <strong>失败数:</strong> {total_failures} | <strong>耗时:</strong> {total_time}s</p>
        </div>
        <h2>测试详情</h2>
        <table>
            <thead>
                <tr>
                    <th>测试套件</th>
                    <th>测试用例</th>
                    <th>状态</th>
                    <th>耗时 (s)</th>
                    <th>文件位置</th>
                    <th>属性</th>
                </tr>
            </thead>
            <tbody>
    """

    # 遍历 testsuite 和 testcase
    for testsuite in root.findall('testsuite'):
        suite_name = testsuite.attrib.get('name', 'Unknown')
        for testcase in testsuite.findall('testcase'):
            case_name = testcase.attrib.get('name', 'Unknown')
            status = testcase.attrib.get('result', 'unknown')
            time_cost = testcase.attrib.get('time', '0')
            file_path = testcase.attrib.get('file', '')
            line = testcase.attrib.get('line', '')
            
            # 提取 properties
            props = []
            properties_node = testcase.find('properties')
            if properties_node is not None:
                for prop in properties_node.findall('property'):
                    name = prop.attrib.get('name', '')
                    value = prop.attrib.get('value', '')
                    props.append(f"{name}: {value}")
            props_str = "<br>".join(props)

            status_class = "pass" if status == "completed" else "fail"
            
            html_content += f"""
                <tr>
                    <td>{suite_name}</td>
                    <td>{case_name}</td>
                    <td class="{status_class}">{status}</td>
                    <td>{time_cost}</td>
                    <td>{os.path.basename(file_path)}:{line}</td>
                    <td class="properties">{props_str}</td>
                </tr>
            """

    html_content += """
            </tbody>
        </table>
    </body>
    </html>
    """

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(html_content)
    print(f"HTML 报告已生成：{output_path}")

if __name__ == "__main__":
    input_file = "E:\\xiaoyao-sdk-c++\\xiaoyao-sdk-cpp\\build\\test_report.xml"
    output_file = "test_report.html"
    
    if os.path.exists(input_file):
        root = parse_xml_report(input_file)
        generate_html_report(root, output_file)
    else:
        print(f"未找到文件：{input_file}")