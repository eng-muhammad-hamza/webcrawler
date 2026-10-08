# analyze_runs.py
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from datetime import datetime
import os

def load_run_data():
    try:
        # Initialize empty DataFrame for history
        history = pd.DataFrame()
        
        # Load current run metrics
        current = pd.read_csv('metrics.csv').set_index('metric')['value']
        
        # Load history if file exists and is not empty
        if os.path.exists('metrics_history.csv') and os.path.getsize('metrics_history.csv') > 0:
            history = pd.read_csv('metrics_history.csv')
            history['timestamp'] = pd.to_datetime(history['timestamp'])
        
        # Prepare current run data
        current_df = pd.DataFrame([{
            'timestamp': pd.to_datetime(datetime.now()),
            'total_runtime': float(current['total_runtime']),
            'url_download': float(current['url_download']),
            'thread_distribution': float(current['thread_distribution']),
            'thread_execution': float(current['thread_execution']),
            'word_count_total': float(current['word_count_total']),
            'top_words_total': float(current['top_words_total']),
            'longest_word_total': float(current['longest_word_total']),
            'urls_processed': int(current['urls_processed']),
            'urls_failed': int(current['urls_failed']),
            'total_bytes': int(current['total_bytes'])
        }])
        
        # Combine with history
        combined = pd.concat([history, current_df], ignore_index=True)
        
        # Ensure proper datetime conversion for all rows
        combined['timestamp'] = pd.to_datetime(combined['timestamp'])
        
        # Save updated history (including current run)
        combined.to_csv('metrics_history.csv', index=False)
        
        return combined
    
    except Exception as e:
        print(f"Error loading data: {str(e)}")
        return None

def create_trend_visualizations():
    data = load_run_data()
    if data is None or len(data) < 1:  # Changed to work with single run
        print("No data available for visualization")
        return

    plt.figure(figsize=(18, 12))
    plt.suptitle("Web Crawler Performance Trends Across Runs", fontsize=16, y=0.98)
    
    # Set style
    sns.set_style("whitegrid")
    palette = sns.color_palette("husl", 8)
    
    # Convert timestamp to shorter format for display
    data['run'] = data['timestamp'].dt.strftime('%m-%d %H:%M')
    
    # 1. System Performance Over Time
    plt.subplot(2, 2, 1)
    sns.lineplot(data=data, x='run', y='total_runtime', 
                marker='o', label='Total Runtime', color=palette[0])
    sns.lineplot(data=data, x='run', y='url_download', 
                marker='o', label='URL Download', color=palette[1])
    sns.lineplot(data=data, x='run', y='thread_execution', 
                marker='o', label='Task Processing', color=palette[2])
    plt.title('System Performance Over Time')
    plt.ylabel('Time (ms)')
    plt.xlabel('Run')
    plt.xticks(rotation=45)
    plt.legend()
    
    # 2. Worker Performance Trends
    plt.subplot(2, 2, 2)
    sns.lineplot(data=data, x='run', y='word_count_total', 
                marker='o', label='Word Count', color=palette[3])
    sns.lineplot(data=data, x='run', y='top_words_total', 
                marker='o', label='Top Words', color=palette[4])
    sns.lineplot(data=data, x='run', y='longest_word_total', 
                marker='o', label='Longest Word', color=palette[5])
    plt.title('Worker Performance Trends')
    plt.ylabel('Total Time (ms)')
    plt.xlabel('Run')
    plt.xticks(rotation=45)
    plt.legend()
    
    # 3. Throughput Metrics
    plt.subplot(2, 2, 3)
    ax3 = plt.gca()
    sns.lineplot(data=data, x='run', y='urls_processed', 
                marker='o', label='URLs Processed', color=palette[6], ax=ax3)
    ax3.set_ylabel('URL Count', color=palette[6])
    ax3.tick_params(axis='y', labelcolor=palette[6])
    
    ax3b = ax3.twinx()
    sns.lineplot(data=data, x='run', y='total_bytes', 
                marker='s', label='Data (KB)', color=palette[7], ax=ax3b)
    ax3b.set_ylabel('Data (KB)', color=palette[7])
    ax3b.tick_params(axis='y', labelcolor=palette[7])
    plt.title('Throughput Metrics')
    plt.xlabel('Run')
    plt.xticks(rotation=45)
    
    # Combine legends
    lines, labels = ax3.get_legend_handles_labels()
    lines2, labels2 = ax3b.get_legend_handles_labels()
    ax3.legend(lines + lines2, labels + labels2)
    
    # 4. Efficiency Metrics
    plt.subplot(2, 2, 4)
    data['bytes_per_url'] = data['total_bytes'] / data['urls_processed']
    data['time_per_url'] = data['total_runtime'] / data['urls_processed']
    
    sns.lineplot(data=data, x='run', y='bytes_per_url', 
                marker='o', label='Bytes/URL', color=palette[0])
    sns.lineplot(data=data, x='run', y='time_per_url', 
                marker='o', label='ms/URL', color=palette[1])
    plt.title('Efficiency Metrics')
    plt.ylabel('Bytes or ms per URL')
    plt.xlabel('Run')
    plt.xticks(rotation=45)
    plt.legend()
    
    plt.tight_layout()
    plt.savefig('performance_trends.png', dpi=300, bbox_inches='tight')
    print("Trend analysis saved as 'performance_trends.png'")
    plt.show()

if __name__ == "__main__":
    create_trend_visualizations()