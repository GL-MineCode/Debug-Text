import os

def file_to_cpp_byte_array(input_file_path, output_file_path=None, array_name="file_data", bytes_per_line=16):
    """
    将任意文件转换为C++字节数组格式
    
    参数:
        input_file_path: 输入文件的路径
        output_file_path: 输出C++代码的文件路径（None则输出到控制台）
        array_name: 生成的C++数组名称
        bytes_per_line: 每行显示的字节数，增强可读性
    """
    # 检查输入文件是否存在
    if not os.path.exists(input_file_path):
        raise FileNotFoundError(f"输入文件不存在: {input_file_path}")
    
    # 以二进制模式读取文件内容
    with open(input_file_path, "rb") as f:
        file_data = f.read()
    
    # 将每个字节转换为0xXX格式的字符串
    byte_strings = [f"0x{byte:02x}" for byte in file_data]
    
    # 按指定行数分组，生成格式化的数组内容
    formatted_lines = []
    for i in range(0, len(byte_strings), bytes_per_line):
        line_bytes = byte_strings[i:i+bytes_per_line]
        line = ", ".join(line_bytes) + ","
        formatted_lines.append("    " + line)
    
    # 拼接完整的C++代码
    cpp_code = f"""// 自动生成的C++字节数组
// 源文件: {input_file_path}
// 文件大小: {len(file_data)} 字节

#include <cstdint>

// 字节数组定义
const uint8_t {array_name}[] = {{
{chr(10).join(formatted_lines)}
}};

// 数组长度常量
const size_t {array_name}_len = sizeof({array_name}) / sizeof({array_name}[0]);
"""
    
    # 输出结果
    if output_file_path:
        with open(output_file_path, "w", encoding="utf-8") as f:
            f.write(cpp_code)
        print(f"C++字节数组已保存到: {output_file_path}")
    else:
        print(cpp_code)

# 示例用法
if __name__ == "__main__":
    # 配置参数
    INPUT_FILE = "D:\\Desktop\\Static Projects\\binFontGen\\font.bin"       # 替换成你的输入文件路径
    OUTPUT_FILE = "D:\\Desktop\\Static Projects\\binFontGen\\fusionpixel_12px_mono.cpp"   # 输出的C++头文件路径
    ARRAY_NAME = "font_data"   # 数组名称
    
    # 执行转换
    try:
        file_to_cpp_byte_array(INPUT_FILE, OUTPUT_FILE, ARRAY_NAME)
    except Exception as e:
        print(f"转换失败: {e}")
