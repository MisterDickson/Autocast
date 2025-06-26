import re
import collections

def parse_s_expression_blocks(content, block_name):
    """
    Parses a file with s-expressions and extracts top-level blocks
    with a given name, e.g., "footprint". This is more robust than a single regex.
    """
    blocks = []
    search_prefix = f"({block_name}"
    search_len = len(search_prefix)
    start_index = 0

    while True:
        try:
            # Find the start of the next block
            block_start = content.index(search_prefix, start_index)
        except ValueError:
            # No more blocks found
            break

        # Found the start, now find the matching end parenthesis
        open_parens = 0
        for i in range(block_start, len(content)):
            if content[i] == '(':
                open_parens += 1
            elif content[i] == ')':
                open_parens -= 1

            if open_parens == 0:
                # Found the complete block
                blocks.append(content[block_start : i + 1])
                start_index = i + 1
                break
        else:
            # If the loop finishes without finding a closing paren, something is wrong
            # Move start_index to avoid an infinite loop
            start_index = block_start + search_len

    return blocks

def get_footprint_details(footprint_str):
    """
    Extracts key details from a footprint string: type, reference, and current position.
    """
    # Extract the footprint type (e.g., "External_Parts:ACPSC04-41SEKWA")
    type_match = re.search(r'\(\s*footprint\s+"([^"]+)"', footprint_str)
    footprint_type = type_match.group(1) if type_match else None

    # Extract the reference designator (e.g., "SEGD1", "R280")
    ref_match = re.search(r'\(\s*property\s+"Reference"\s+"([^"]+)"', footprint_str)
    reference = ref_match.group(1) if ref_match else None

    # Extract the (at x y rotation) line using a more robust regex
    at_match = re.search(r'\(at\s+([-\d.]+)\s+([-\d.]+)(?:\s+([-\d.]+))?\)', footprint_str)
    position = at_match.groups() if at_match else (None, None, None)

    # Extract the full (at ...) string to make replacement easier
    at_string = at_match.group(0) if at_match else None

    return {
        'type': footprint_type,
        'reference': reference,
        'x': float(position[0]) if position[0] is not None else None,
        'y': float(position[1]) if position[1] is not None else None,
        'rotation': float(position[2]) if position[2] is not None else None,
        'at_string': at_string,
        'original_string': footprint_str
    }

def sort_components(components, prefix):
    """
    Sorts components based on the numeric part of their reference designator.
    For example, "SEGD1", "SEGD2", ..., "SEGD28".
    """
    return sorted(
        [c for c in components if c['reference'] and c['reference'].startswith(prefix)],
        key=lambda x: int(re.search(r'\d+', x['reference']).group())
    )

def main():
    """
    Main function to execute the placement logic.
    """
    input_file = 'Display.kicad_pcb'  # Use the uploaded filename
    output_file = 'Display_modified.kicad_pcb'

    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            original_content = f.read()
    except FileNotFoundError:
        print(f"Error: The file '{input_file}' was not found.")
        print("Please make sure the script is in the same directory as your KiCad file.")
        return

    # Parse all footprints from the file
    footprint_strings = parse_s_expression_blocks(original_content, "footprint")
    if not footprint_strings:
        print("Could not parse any footprints from the file. Please check the file format.")
        return

    all_components = [get_footprint_details(fp) for fp in footprint_strings]

    # Filter and sort displays and resistors
    displays = sort_components([c for c in all_components if c['type'] == "External_Parts:ACPSC04-41SEKWA"], 'SEGD')
    resistors = sort_components([c for c in all_components if "Resistor_SMD" in str(c.get('type'))], 'R')

    print(f"Found {len(displays)} displays to process.")
    print(f"Found {len(resistors)} resistors to process.")

    if not displays:
        print("Error: No displays with the specified footprint 'External_Parts:ACPSC04-41SEKWA' were found.")
        return
    if len(resistors) < len(displays) * 15:
        print(f"Warning: Found {len(resistors)} resistors, but expected at least {len(displays) * 15}.")


    # --- Placement Logic ---
    start_x = 5.7
    start_y = 199.1
    spacing = 11.4

    resistor_relative_coords = [
        (-5.7, -4.0, 90), (-5.7, -1.4, 90), (-5.7, 1.2, -90),
        (-5.7, 3.8, -90), (-5.7, 6.5, -90), (-5.7, -9.2, 90),
        (-5.7, -6.6, 90), (-2.5, -11.0, 0), (1.5, -11.0, 0),
        (3.9, -11.0, 0), (1.3, 10.9, 0), (-1.0, 10.9, 0),
        (-3.3, 10.9, 0), (-5.7, 9.2, -190), (3.7, 10.9, 180)
    ]

    modified_content = original_content

    # Process each display
    for i, display in enumerate(displays):
        # Calculate new display position
        new_display_x = start_x + (i * spacing)
        new_display_y = start_y

        # Format the new (at ...) string for the display
        rot_str = f" {display['rotation']}" if display['rotation'] is not None else ""
        new_at_str = f"(at {new_display_x:.4f} {new_display_y:.4f}{rot_str})"

        # Replace the old 'at' line with the new one
        if display['at_string'] and display['original_string'] in modified_content:
            new_footprint_block = display['original_string'].replace(display['at_string'], f"    {new_at_str}", 1)
            modified_content = modified_content.replace(display['original_string'], new_footprint_block, 1)
            print(f"Placed {display['reference']:<8} at X={new_display_x:.2f}")

        # Process corresponding resistors for this display
        start_resistor_index = i * 15
        end_resistor_index = start_resistor_index + 15
        current_display_resistors = resistors[start_resistor_index:end_resistor_index]

        for j, resistor in enumerate(current_display_resistors):
            if j >= len(resistor_relative_coords):
                print(f"Warning: Not enough relative coordinates for {resistor['reference']}")
                continue

            rel_x, rel_y, rel_rot = resistor_relative_coords[j]

            # Calculate new resistor position
            new_resistor_x = new_display_x + rel_x
            new_resistor_y = new_display_y + rel_y

            # Format the new (at ...) string
            new_resistor_at_str = f"(at {new_resistor_x:.4f} {new_resistor_y:.4f} {rel_rot})"

            if resistor['at_string'] and resistor['original_string'] in modified_content:
                new_resistor_block = resistor['original_string'].replace(resistor['at_string'], f"    {new_resistor_at_str}", 1)
                modified_content = modified_content.replace(resistor['original_string'], new_resistor_block, 1)
                # print(f"  - Placed {resistor['reference']:<8} at X={new_resistor_x:.2f}")

    # Write the modified content to a new file
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(modified_content)

    print(f"\nProcessing complete. The modified file has been saved as '{output_file}'")

if __name__ == '__main__':
    main()
